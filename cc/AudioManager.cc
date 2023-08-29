#include "AudioManager.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

AudioManager::AudioManager(bool verbose, bool superVerbose, bool audioPlayerVerbose, bool audioProcessorVerbose, 
    MasterClock& mc, KeyboardEvent& kb, LooperManager& lm,
    const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs, const YAML::Node& audioMixerConfig,
    std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex) :
    verbose(verbose), audioPlayerVerbose(audioPlayerVerbose), 
    audioProcessorVerbose(audioProcessorVerbose), superVerbose(superVerbose),
    masterClock(mc), keyboardEvent(kb), looperManager(lm),
    bpm(mc.getBPM()), beatDivisions(mc.getBeatDivisions()), 
    beatDurationAsDuration(mc.fetchDivisionDurationAsDuration()),
    audioProcessor(audioProcessorVerbose), stringBoolPairs(stringBoolPairs),
    managerThreadCV(managerThreadCV), managerThreadMutex(managerThreadMutex),
    runAudioPlaybackThread(false), addLooper(false) {
    // Initialize SDL_mixer
    int mixerSampleRate = audioMixerConfig["mixer_sample_rate"].as<int>();
    int mixerChannels = audioMixerConfig["mixer_channels"].as<int>();
    int mixerBufferSize = audioMixerConfig["mixer_buffer_size"].as<int>();
    int mixerAudioFormatSign = audioMixerConfig["audio_format"].as<int>();
    int mixerAudioFormat;
    if (mixerAudioFormatSign == 1) {
        mixerAudioFormat = AUDIO_S16LSB; // WSL
    } else if (mixerAudioFormatSign == 2) {
        mixerAudioFormat = AUDIO_F32SYS; // PI
    }

    if (Mix_OpenAudio(mixerSampleRate, mixerAudioFormat, mixerChannels, mixerBufferSize) < 0) {
        printf("---SDL_mixer initialization failed: %s\n", Mix_GetError());
        return;
    } else {
        printf("   AudioManager::SDL_mixer initialization successful.\n");
    }
    if (verbose) {
        printf("      AudioManager::Constructred.\n");
    }
}

AudioManager::~AudioManager() {
    unschedulePlayback();
    Mix_CloseAudio();
}
// Getter/Setter Function Section
//###################################################################################################################
void AudioManager::setCurrentFunction(std::string function) {
    if (verbose) {
        printf("   AudioManager::setCurrentFunction::Updated: %s.\n", function.c_str());
    }
    currentFunction = function;
}

void AudioManager::setKeypadReady(bool stateUpdate) {
    if (verbose && superVerbose) {
        printf("   AudioManager::setKeypadReady::State Update: %d.\n", stateUpdate);
    }
    addLooper = stateUpdate;
}
// Thread Managment SECTION
// #################################################################################################
void AudioManager::schedulePlayback() {
    if (verbose) {
        printf("      AudioManager::startHandlingPlayback::Schedulong Audio Playback Task.\n");
    }
    masterClock.setRuntimeTasks([&]() {
        this->audioPlaybackTask(); // Call the function you want to execute
    });
    
}

void AudioManager::unschedulePlayback() {
    if (verbose) {
        printf("       AudioManager::startHandlingPlayback::Signaling to Stop Audio Playback Thread.\n");
    }
    std::lock_guard<std::mutex> lock(playerMapMutex);
    for (const auto& config : noteConfigurations) {
        removeAudioPlayer(config.keycode); // Clean up each audio player
    }

}
// Audio Player Section
//###################################################################################################################
bool AudioManager::addAudioPlayer(const char* filepath, NoteConfiguration config) {
    if (verbose) {
        printf("      AudioManager::addAudioPlayer::FilePath: %s.\n", config.noteName.c_str());
    }

    try {
        std::string functionAssignment = config.functionAssignment;
        std::string noteName = config.noteName;

        // Create and manage AudioPlayer instance using unique_ptr
        AudioPlayer* player = new AudioPlayer(audioPlayerVerbose, filepath, audioProcessor);

        // Lock the playerMapMutex to safely modify playerMap
        {
            std::lock_guard<std::mutex> lock(playerMapMutex);
            playerMap.emplace(noteName, std::make_pair(functionAssignment, player));
        }
        // Lock the noteConfigurationsMutex to safely modify noteConfigurations
        {
            std::lock_guard<std::mutex> lock(noteConfigurationsMutex);
            noteConfigurations.push_back(config);
        }

        return true;
    } catch (const std::exception& e) {
        // Handle the exception, e.g., log the error
        printf("      ---AudioManager::addAudioPlayer::Failed to create AudioPlayer: %s\n", e.what());
        return false;
    }
}

bool AudioManager::removeAudioPlayer(SDL_Scancode keyCode) {
    std::lock_guard<std::mutex> lock(playerMapMutex);
    const NoteConfiguration* configToRemove = nullptr;
    auto foundConfig = std::find_if(noteConfigurations.begin(), noteConfigurations.end(),
        [&](const auto& config) {
            return config.functionAssignment == currentFunction && config.keycode == keyCode;
        });
        
    if (foundConfig != noteConfigurations.end()) {
        configToRemove = &(*foundConfig); // Store the pointer to the found configuration
        playerMap.erase(configToRemove->noteName);
        return true;
    }
    return false;
}

AudioPlayer* AudioManager::getAudioPlayer(SDL_Scancode keyCode) {
    std::lock_guard<std::mutex> lock(playerMapMutex);
    for (const auto& config : noteConfigurations) {
        if (config.functionAssignment == currentFunction && config.keycode == keyCode) {
            if (verbose) {
                printf("   AudioManager::getAudioPlayer:Note Found: %s.\n", config.noteName.c_str());
            }
            auto it = playerMap.find(config.noteName.c_str());
            if (it != playerMap.end() && it->second.first == currentFunction) {
                return it->second.second;
            }
        }
    }

    // Return nullptr or handle the case as needed when not found
    return nullptr;
}

void AudioManager::audioPlaybackTask() {
    if (audioPlayerVerbose && superVerbose) {
        printf("      AudioManager::audioPlaybackHandler::stringBoolPairs:\n");
        printf("   addLooper: %s.\n", addLooper ? "true" : "false");
        for (const auto& pair : stringBoolPairs) {
            printf("   %s: %s\n", pair.first.c_str(), *(pair.second.first) ? "true" : "false");
        }
    }

    if (!keyboardEvent.isScancodeDataEmpty()) {
        if (verbose && audioPlayerVerbose) {
            printf("      AudioManager::audioPlaybackHandler::Keyboard event found.\n");
        }
        std::string noteInfoString = "Playing Notes: ";
        const std::unordered_set<SDL_Scancode>& scancodeData = keyboardEvent.getScancodeData();

        bool activeLooper = false;
        std::string keypadIDString;
        double loopDuration = 0.0;

        // Find the active looper (if any) from stringBoolPairs
        for (const auto& pair : stringBoolPairs) {
            bool loopState = *(pair.second.first); // Dereference the bool pointer
            if (loopState) {
                activeLooper = loopState;
                keypadIDString = pair.first;
                loopDuration = pair.second.second;
                break; // Break after finding the active looper
            }
        }
        if (verbose && addLooper) {
            printf("      AudioManager::playAudio::LooperState: %d.\n", activeLooper);
            printf("      AudioManager::playAudio::Keypad Number: %s.\n", keypadIDString.c_str());
            printf("      AudioManager::playAudio::Duration: %f.\n", loopDuration);
        }
        std::for_each(scancodeData.begin(), scancodeData.end(), [&](const auto& keycode) {
            AudioPlayer* player = getAudioPlayer(keycode);
            if (player) {
                player->playAudio();
                if (activeLooper && addLooper) {
                    bool success = looperManager.addAudioLooper(keypadIDString, player, loopDuration);
                    if (verbose) {
                        printf("   AudioManager::audioPlaybackTask::addLooper success: %d.\n", success);
                    }
                }
            } else {
                if (verbose && audioPlayerVerbose) {
                    printf("      AudioManager::audioPlaybackHandler::Note '%d' not found in the player map.\n", keycode);
                }
            }
        });
        keyboardEvent.clearScancodeData();
        
        // masterClock.startTimer("PLAYPROCESS", false);
        // printf("   ---%s", masterClock.getDurationString("PLAYPROCESS").c_str());
        // // window->provideNoteInfo(noteInfoString);
    }
}
