// Manager.cc
#include "Manager.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <unordered_set>

Manager::Manager(MasterClock& mc,
    const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
    const YAML::Node& verbosity,
    const YAML::Node& notesConfig, const YAML::Node& windowConfig,
    const YAML::Node& audioMixerConfig,
    bool sV, bool tV) :
    masterClock(mc), currentFunction("FN10"),
    verbose(verbosity["managerVerbose"].as<bool>()),
    stringBoolPairs(stringBoolPairs), notesConfig(notesConfig),
    keyboardEvent(masterClock, verbosity["keyboardEventVerbose"].as<bool>(), tV, sV),
    looperManager(masterClock, keyboardEvent, stringBoolPairs,
        verbosity["looperVerbosity"], sV, tV),
    graphicManager(verbosity["graphicVerbosity"], sV, tV,
         masterClock, windowConfig),
    audioManager(masterClock, keyboardEvent, looperManager,
        stringBoolPairs, verbosity["audioVerbosity"], audioMixerConfig, sV) {
    if (verbose) {
        printf("   Manager::Constructor Entered.\n");
    }
    setNotesConfig();
    mixerBufferSize = audioMixerConfig["mixer_buffer_size"].as<int>(0);  // unused by the GStreamer engine; optional
    scheduleAudioLooperTask();
    scheduleAudioPlaybackTask();
    // No keyboard thread any more: SDL events are pumped on the main thread
    // in runEventLoop().
    scheduleupdateStates();
    // startAnimationThread();
    if (verbose) {
        printf("   Manager::Constructed.\n");
    }
}

Manager::~Manager() {
    joinManagerThread();
}

void Manager::joinManagerThread() {
    printf("   Manager::joinManagerThread::Entered.\n");
    keyboardEvent.stopHandlingEvents();
    graphicManager.stopAnimationWindow();
    printf("   Manager::joinManagerThread::KeyboardEventThread Down.\n");
    audioManager.unschedulePlayback();
    printf("   Manager::joinManagerThread::unschedulePlayback Done.\n");
}
// Getter/Setter Section
//###################################################################################################################
void Manager::setNotesConfig() {
    // Process the notes map
    if (verbose) {
        printf("   Manager::setNotesConfig::Entered.\n");
    }
    const size_t total = notesConfig.size();
    size_t loaded = 0, failed = 0, index = 0;
    for (const auto& note : notesConfig) {
        NoteConfiguration config;
        config.noteName = note.first.as<std::string>();
        const YAML::Node& noteConfig = note.second;
        std::string filepath = noteConfig["filepath"].as<std::string>();
        config.keycode = static_cast<SDL_Scancode>(noteConfig["keycode"].as<int>());
        config.functionAssignment = noteConfig["fnNumber"].as<std::string>();
        if (audioManager.addAudioPlayer(filepath.c_str(), config)) {
            ++loaded;
        } else {
            ++failed;
        }
        // Keep the window alive while loading: without this the window sits
        // unserviced, shows a frozen copy of the desktop, and the OS offers
        // to force-quit it.
        ++index;
        SDL_PumpEvents();
        graphicManager.renderFrame("Loading samples " + std::to_string(index) + " / " + std::to_string(total));
    }
    printf("   Manager::setNotesConfig::Loaded %zu of %zu samples (%zu failed).\n", loaded, total, failed);
}

void Manager::setFunction() {
    bool didChangeLock = false;
    std::string updatedFunction = keyboardEvent.getFunctionState();

    if (!(currentFunction == updatedFunction)) {
        didChangeLock = true;
        currentFunction = updatedFunction;
    }
    if (didChangeLock) {
        audioManager.setCurrentFunction(currentFunction);
        keyboardEvent.functionFetchReset();
    }
}

void Manager::updateStates() {
    for (int index = 1; index <= 9; index++) {
        std::string key = "KP" + std::to_string(index);
        auto it = stringBoolPairs.find(key);
        if (it != stringBoolPairs.end()) {
            *(it->second.first) = keyboardEvent.getKeypadStates(index - 1);
        }
    }
    if (keyboardEvent.getKeypadStates(-1)) {
        audioManager.setKeypadReady(keyboardEvent.getKeypadStates(-1));
    } else {
        audioManager.setKeypadReady(false);
    }

    if (keyboardEvent.getKeypadStates(-2)) {
        looperManager.setRemoveLooper(keyboardEvent.getKeypadStates(-2));
    }
    setFunction();
}
// Manager Thread Section
//###################################################################################################################
void Manager::scheduleAudioLooperTask() {
    if (verbose) {
        printf("   Manager::startAudioLooperThread.\n");
    }
    looperManager.scheduleLooperTask();
}

void Manager::scheduleAudioPlaybackTask() {
    if (verbose) {
        printf("   Manager::scheduleAudioPlaybackTask.\n");
    }
    audioManager.schedulePlayback();
}

void Manager::startAnimationThread() {
    if (verbose) {
        printf("   Manager::startAnimationThread.\n");
    }
    graphicManager.startAnimationWindow();
}

void Manager::runEventLoop() {
    SDL_Event event;
    while (!keyboardEvent.quitRequested()) {
        // Wake on any event, or every ~16 ms to redraw (~60 fps).
        if (SDL_WaitEventTimeout(&event, 16)) {
            keyboardEvent.processEvent(event);
            while (SDL_PollEvent(&event)) {   // drain anything else queued
                keyboardEvent.processEvent(event);
            }
        }
        graphicManager.renderFrame(buildStatusLabel());
    }
    printf("   Manager::runEventLoop::Window closed.\n");
}

std::string Manager::buildStatusLabel() {
    std::string label = "Melydy   " + keyboardEvent.getFunctionState() + "   loops:";
    bool any = false;
    for (int index = 1; index <= 9; index++) {
        if (keyboardEvent.getKeypadStates(index - 1)) {
            label += " KP" + std::to_string(index);
            any = true;
        }
    }
    if (!any) {
        label += " none";
    }
    // Output meter: 10 bars = full scale. Bars appearing when you hit a key
    // means sound is leaving the mixer.
    int bars = static_cast<int>(audioManager.getOutputLevel() * 10.0f + 0.5f);
    if (bars > 10) bars = 10;
    label += "   out [" + std::string(bars, '|') + std::string(10 - bars, '.') + "]";
    label += "   voices " + std::to_string(audioManager.getActiveVoices()) + "/" +
        std::to_string(audioManager.getVoiceCount());
    return label;
}

void Manager::startKeyboardThread() {
    if (verbose) {
        printf("   Manager::startKeyboardThread.\n");
    }
    keyboardEvent.startHandlingEvents();
}

void Manager::scheduleupdateStates() {
    if (verbose) {
        printf("   Manager::scheduleupdateStates.\n");
    }
    masterClock.setRuntimeTasks([&]() {
        this->updateStates(); 
    });
}

