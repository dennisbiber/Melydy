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
    stopHandlingLooping();
}
// Getter/Setter Function SECTION
// #################################################################################################
void LooperManager::setRemoveLooper(bool removeState) {
    removeLooper = removeState;
}
// Thread Managment SECTION
// #################################################################################################
void LooperManager::startHandlingLooping() {
    if (verbose) {
        printf("      LooperManager::startHandlingLooping::Starting Audio Looper Thread.\n");
    }
    audioLooperThread = std::thread(&LooperManager::audioLooperHandler, this);
    
}

void LooperManager::stopHandlingLooping() {
    if (verbose) {
        printf("       LooperManager::stopHandlingLooping::Signaling to Stop Audio Looper Thread.\n");
    }
    runAudioLooperThread = false;

    if (audioLooperThread.joinable()) {
        audioLooperThread.join();  // Wait for the thread to finish
    }
}

// Audio Looper Section
//###################################################################################################################
void LooperManager::audioLooperHandler() {
    if (verbose) {
        printf("      LooperManager::audioLooperHandler::Entered.\n");
    }
    runAudioLooperThread = true;
    while (runAudioLooperThread) {
        std::unique_lock<std::mutex> waitLock(managerThreadMutex);
        managerThreadCV.wait(waitLock);
        if (!audioLoopers.empty() && removeLooper) {
            if (verbose && audioLooperVerbose && superVerbose) {
                printf("   LooperManager::Audio Manager Thread::Looper Ready.\n");
            }
            std::string keypadIDStringToRemove;
            
            // Find the keypadIDString that needs to be removed using std::find_if
            auto foundPair = std::find_if(stringBoolPairs.begin(), stringBoolPairs.end(),
                [](const auto& pair) {
                    return *pair.second.first;
                });
                
            if (foundPair != stringBoolPairs.end()) {
                keypadIDStringToRemove = foundPair->first;
            }
            
            auto it = audioLoopers.begin();
            while (it != audioLoopers.end()) {
                if (verbose) {
                    printf("KEY STRING> %s.\n", keypadIDStringToRemove.c_str());
                }
                if (it->second.first == keypadIDStringToRemove) {
                    if (it->second.second.second->checkIfLooping()) {
                        it->second.second.second->stopLoop();
                    }
                    it = audioLoopers.erase(it);
                } else {
                    ++it;
                }
            }
            removeLooper = false;
        }
    }
}


bool LooperManager::addAudioLooper(const std::string& keypadIDStr, AudioPlayer* player, double loopDuration, std::string keypadString) {
    int keypadID = 0;
    std::lock_guard<std::mutex> lock(audioLoopersMutex);
    if (!audioLoopers.empty()) {
        // Find the highest integer key and add 1 to get the new keypadID
        auto maxEntry = std::max_element(audioLoopers.begin(), audioLoopers.end(),
            [](const auto& entry1, const auto& entry2) {
                return entry1.first < entry2.first;
            });

        keypadID = maxEntry->first + 1;
    }
    if (verbose && audioLooperVerbose) {
        printf("      LooperManager::addAudioLooper::Looper ID: %d\n", keypadID);
        printf("      LooperManager::addAudioLooper::Looper filepath from player: %s\n", player->getFilePath().c_str());
        printf("      LooperManager::addAudioLooper::Looper Duration %f.\n", loopDuration);
    }
    try {
        auto audioLooper = std::make_shared<AudioLooper>(masterClock, player, loopDuration, audioLooperVerbose, keypadString);
        audioLooper->startLoop();
        audioLoopers[keypadID] = {keypadIDStr, {false, audioLooper}}; // Set both the integer ID and the AudioLooper pointer
        return true;
    } catch (const std::exception& e) {
        // Handle the exception, e.g., print an error message or log the error
        printf("   ---LooperManager::addAudioLooper::Failed to create AudioLooper: %s\n", e.what());
        return false;
    }
}
