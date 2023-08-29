// Manager.cc
#include "Manager.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <unordered_set>

Manager::Manager(MasterClock& mc,
    bool verbose, bool superVerbose, bool timeVerbose,
    bool looperManagerVerbose, bool audioLooperVerbose, bool graphicLooperVerbose,
    bool graphicManagerVerbose, bool graphicPlayerVerbose, bool graphicProcessorVerbose,
    bool audioManagerVerbose, bool audioPlayerVerbose, bool audioProcessorVerbose,
    bool keyboardEventVerbose, 
    const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
    const YAML::Node& notesConfig, const YAML::Node& windowConfig,
    const YAML::Node& audioMixerConfig) :
    masterClock(mc),
    looperManagerVerbose(looperManagerVerbose),
    audioLooperVerbose(audioLooperVerbose), graphicLooperVerbose(graphicLooperVerbose),
    audioManagerVerbose(audioManagerVerbose), graphicManagerVerbose(graphicManagerVerbose),
    audioPlayerVerbose(audioPlayerVerbose), audioProcessorVerbose(audioProcessorVerbose),
    graphicPlayerVerbose(graphicPlayerVerbose), graphicProcessorVerbose(graphicProcessorVerbose),
    currentFunction("FN10"),
    verbose(verbose), superVerbose(superVerbose), timeVerbose(timeVerbose),
    stringBoolPairs(stringBoolPairs), notesConfig(notesConfig),
    keyboardEvent(masterClock, keyboardEventVerbose, timeVerbose, superVerbose),
    looperManager(masterClock, keyboardEvent, stringBoolPairs,
        looperManagerVerbose, superVerbose, timeVerbose, audioLooperVerbose, graphicLooperVerbose,
        managerThreadCV, managerThreadMutex),
    audioManager(audioManagerVerbose, superVerbose, audioPlayerVerbose, audioProcessorVerbose, 
        masterClock, keyboardEvent, looperManager, stringBoolPairs, audioMixerConfig, 
        managerThreadCV, managerThreadMutex), 
    graphicManager(graphicManagerVerbose, superVerbose, graphicPlayerVerbose, graphicProcessorVerbose, timeVerbose,
         masterClock, windowConfig, managerThreadCV, managerThreadMutex) {
    if (verbose) {
        printf("   Manager::Constructor Entered.\n");
    }
    setNotesConfig();
    if (verbose) {
        printf("   Manager::BPM and NotesConfig set.\n");
    }
    mixerBufferSize = audioMixerConfig["mixer_buffer_size"].as<int>();
    if (verbose) {
        printf("   Manager::Audio Information set.\n");
    }
    scheduleAudioLooperTask();
    scheduleAudioPlaybackTask();
    startKeyboardThread();
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
    for (const auto& note : notesConfig) {
        NoteConfiguration config;
        config.noteName = note.first.as<std::string>();
        const YAML::Node& noteConfig = note.second;
        std::string filepath = noteConfig["filepath"].as<std::string>();
        config.keycode = static_cast<SDL_Scancode>(noteConfig["keycode"].as<int>());
        config.functionAssignment = noteConfig["fnNumber"].as<std::string>();
        audioManager.addAudioPlayer(filepath.c_str(), config);
    }
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

