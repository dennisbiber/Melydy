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
    startAudioLooperThread();
    startAudioPlaybackThread();
    startKeyboardThread();
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
    audioManager.setAudioPlabackThreadStatus(false);
    printf("   Manager::joinManagerThread::AudioPlaybackThread Down.\n");
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

void Manager::loopSetter() {
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
}
// Manager Thread Section
//###################################################################################################################
void Manager::startAudioLooperThread() {
    if (verbose) {
        printf("   Manager::startAudioLooperThread.\n");
    }
    looperManager.startHandlingLooping();
}

void Manager::startAudioPlaybackThread() {
    if (verbose) {
        printf("   Manager::startAudioPlaybackThread.\n");
    }
    audioManager.startHandlingPlayback();
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

void Manager::busyWaitHack() {
    bool run = true;
    while (true) {
        if (verbose) {
            printf("   Manager::startAudioProcessing::Waiting for beatTime Array to fill.\n");
        }
        // Check if nextDivisionTime is a valid time_point
        if (masterClock.getDivisionTimePoint(3).time_since_epoch().count() < 0) {
            // Invalid time_point, sleep for 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            printf("   Manager::startAudioProcessing::Valid time_point found.\n");
            break;
        }
    }
    while (run) {
        if (verbose && timeVerbose) {
            masterClock.startTimer("LOOP TIMER", true);
        }
        masterClock.waitForBufferUpdate();
        setFunction();
        loopSetter();
        std::chrono::high_resolution_clock::time_point nextDivisionTime = masterClock.getDivisionTimePoint(3);
        std::chrono::high_resolution_clock::time_point currentTime = masterClock.getCurrentTime();
        std::chrono::high_resolution_clock::duration timeToWait = nextDivisionTime - currentTime;
        if (verbose && superVerbose) {
            auto timeToWaitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(timeToWait);
            if (timeVerbose) {
                printf("Next Division Time: %lld units\n", nextDivisionTime.time_since_epoch().count());
                printf("Current Time: %lld units\n", currentTime.time_since_epoch().count());
            }
            printf("   Manager::Manager Thread::timeToWait: %lld ms\n", timeToWaitDuration.count());
        }
        if (currentTime >= nextDivisionTime) {
            if (verbose && timeVerbose) {
                printf("   Manager::Loop crashed.\n");
                masterClock.startTimer("LOOP TIMER", false);
                printf("%s.\n", masterClock.getDurationString("LOOP TIMER").c_str());
            }
            if (superVerbose) {
                printf("   Manager::Manager Thread::timeToWait was less than zero.\n");
            }
            // Release
            run = false;
        } else if (masterClock.getQuitState()) {
            if (verbose) {
                printf("   Manager::Manager Thread::Clock Signaled to close Manager.\n");
            }
            run = false;
        } else {
            managerThreadCV.notify_all();
            std::this_thread::sleep_until(nextDivisionTime - std::chrono::milliseconds(4));
            if (verbose && timeVerbose) {
                masterClock.startTimer("LOOP TIMER", false);
                printf("%s.\n", masterClock.getDurationString("LOOP TIMER").c_str());
            }
        }
    }
    printf("   Manager::busyWaitHack::While Loop Exited.\n");
}
