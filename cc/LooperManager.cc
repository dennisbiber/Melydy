// LooperManager.cc
#include "LooperManager.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <unordered_set>

LooperManager::LooperManager(MasterClock& mc, KeyboardEvent& kb, 
    const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
    bool verbose, bool superVerbose, bool timeVerbose,
    bool audioLooperVerbose, bool graphicLooperVerbose,
    std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex) :
    masterClock(mc), keyboardEvent(kb),
    verbose(verbose), superVerbose(superVerbose), timeVerbose(timeVerbose), 
    audioLooperVerbose(audioLooperVerbose), graphicLooperVerbose(graphicLooperVerbose),
    stringBoolPairs(stringBoolPairs),
    addLooper(false), removeLooper(false), runAudioLooperThread(false),
    managerThreadCV(managerThreadCV), managerThreadMutex(managerThreadMutex) {
}

LooperManager::~LooperManager() {
}
// Getter/Setter Function SECTION
// #################################################################################################
void LooperManager::setRemoveLooper(bool removeState) {
    removeLooper = removeState;
}
// Thread Managment SECTION
// #################################################################################################
void LooperManager::scheduleLooperTask() {
    if (verbose) {
        printf("      LooperManager::scheduleLooperTask::Starting Audio Looper Task.\n");
    }
    masterClock.setRuntimeTasks([&]() {
        this->audioLooperTask(); // Call the function you want to execute
    });
    
}

// Audio Looper Section
//###################################################################################################################
void LooperManager::audioLooperTask() {
    if (!audioLoopers.empty() && removeLooper) {
        if (verbose) {
            printf("      LooperManager::audioLooperTask::Entered.\n");
        }
        std::string keypadIDStringToRemove;
        
        // Find the keypadIDString that needs to be removed using std::find_if
        auto foundPair = std::find_if(stringBoolPairs.begin(), stringBoolPairs.end(),
            [](const auto& pair) {
                if (pair.second.first) { // Check if the pointer is valid
                    return *pair.second.first;
                }
                return false;
            });
            
        if (foundPair != stringBoolPairs.end()) {
            keypadIDStringToRemove = foundPair->first;
        
            // Remove all loopers with the specified keypadIDString
            removeAudioLoopers(keypadIDStringToRemove);
        }
        removeLooper = false;
    }
}

void LooperManager::removeAudioLoopers(const std::string& keypadIDStr) {
    auto range = keypadIDToLoopers.equal_range(keypadIDStr);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->checkIfLooping()) {
            masterClock.removeBatchFromQueue(it->second->getIDTag());
            it->second->stopLoop();
        }
    }
    keypadIDToLoopers.erase(range.first, range.second);
    audioLoopers.erase(keypadIDStr);
}

bool LooperManager::addAudioLooper(const std::string& keypadIDStr, AudioPlayer* player, double loopDuration) {
    if (verbose && audioLooperVerbose) {
        printf("      LooperManager::addAudioLooper::Looper ID: %s\n", keypadIDStr.c_str());
        printf("      LooperManager::addAudioLooper::Looper Duration %f.\n", loopDuration);
    }
    try {
        auto audioLooper = std::make_shared<AudioLooper>(masterClock, player, loopDuration, audioLooperVerbose, keypadIDStr);
        std::string idTag = audioLooper->startLoop();
        
        // Add the looper to both data structures
        audioLoopers.emplace(keypadIDStr, AudioLooperInfo{keypadIDStr, audioLooper->checkIfLooping(), audioLooper});
        keypadIDToLoopers.emplace(keypadIDStr, audioLooper);
        
        return true;
    } catch (const std::exception& e) {
        // Handle the exception, e.g., print an error message or log the error
        printf("   ---LooperManager::addAudioLooper::Failed to create AudioLooper: %s\n", e.what());
        return false;
    }
}
