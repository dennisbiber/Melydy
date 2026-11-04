#include "AudioManager.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

GstAudioEngine::Config AudioManager::makeEngineConfig(const YAML::Node& audioMixerConfig, bool verbose) {
    // Same YAML section as before. mixer_buffer_size / audio_format are no
    // longer needed: buffer size no longer decides when a sound starts, and the
    // engine always mixes in 32-bit float.
    GstAudioEngine::Config config;
    config.sampleRate = audioMixerConfig["mixer_sample_rate"].as<int>(48000);
    config.voices = audioMixerConfig["mixer_channels"].as<int>(32);
    config.sink = audioMixerConfig["gst_sink"].as<std::string>("autoaudiosink");
    config.mixerLatencyMs = audioMixerConfig["gst_mixer_latency_ms"].as<int>(10);
    config.useSinkClock = audioMixerConfig["gst_use_sink_clock"].as<bool>(true);
    config.feedAheadMs = audioMixerConfig["gst_feed_ahead_ms"].as<int>(10);
    config.verbose = verbose;
    return config;
}

AudioManager::AudioManager(MasterClock& mc, KeyboardEvent& kb, LooperManager& lm,
    const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
    const YAML::Node& audioVerbosity, const YAML::Node& audioMixerConfig, bool sV) :
    engine(makeEngineConfig(audioMixerConfig, audioVerbosity["audioManagerVerbose"].as<bool>())),
    masterClock(mc), keyboardEvent(kb), looperManager(lm),
    audioProcessor(audioVerbosity["audioProcessorVerbose"].as<bool>()),
    audioPlayerVerbose(audioVerbosity["audioPlayerVerbose"].as<bool>()),
    stringBoolPairs(stringBoolPairs),
    verbose(audioVerbosity["audioManagerVerbose"].as<bool>()), superVerbose(sV),
    runAudioPlaybackThread(false), addLooper(false),
    bpm(mc.getBPM()), beatDurationAsDuration(mc.fetchDivisionDurationAsDuration()),
    beatDivisions(mc.getBeatDivisions()) {
    if (verbose) {
        printf("   AudioManager::AudioManager::Entered.\n");
    }
    if (!engine.start()) {
        printf("---GStreamer audio engine failed to start.\n");
        return;
    }
    // The scheduler runs each tick `lookahead` early so its buffers reach
    // audiomixer before their timestamp comes due.
    // Notes must be scheduled beyond what the feeder has already committed
    // (feed-ahead + one 10 ms chunk + scheduler jitter).
    int feedAheadMs = audioMixerConfig["gst_feed_ahead_ms"].as<int>(10);
    int lookaheadMs = std::max(audioMixerConfig["scheduler_lookahead_ms"].as<int>(50), feedAheadMs + 40);
    masterClock.setLookahead(std::chrono::milliseconds(lookaheadMs));
    printf("   AudioManager::GStreamer audio engine running (lookahead %d ms).\n", lookaheadMs);
    if (verbose) {
        printf("      AudioManager::Constructred.\n");
    }
}

AudioManager::~AudioManager() {
    unschedulePlayback();
    engine.shutdown();
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
    // Free every player. This used to hold playerMapMutex and then call
    // removeAudioPlayer(), which locks the same mutex again: a self-deadlock
    // that only showed up once shutdown started running the destructors.
    // (Buffers still queued in GStreamer keep their own reference to the
    // samples, so deleting players here is safe.)
    std::lock_guard<std::mutex> lock(playerMapMutex);
    for (auto& entry : playerMap) {
        delete entry.second.second;
    }
    playerMap.clear();
    noteConfigurations.clear();
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
        AudioPlayer* player = new AudioPlayer(audioPlayerVerbose, filepath, audioProcessor, engine);

        // Lock the playerMapMutex to safely modify playerMap
        {
            std::lock_guard<std::mutex> lock(playerMapMutex);
            playerMap.emplace(noteName, std::make_pair(functionAssignment, player));
        }
        noteConfigurations.push_back(config);

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
    if (superVerbose) {
        printf("      AudioManager::audioPlaybackHandler::stringBoolPairs:\n");
        printf("   addLooper: %s.\n", addLooper ? "true" : "false");
        for (const auto& pair : stringBoolPairs) {
            printf("   %s: %s\n", pair.first.c_str(), *(pair.second.first) ? "true" : "false");
        }
    }

    if (!keyboardEvent.isScancodeDataEmpty()) {
        if (verbose) {
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
                // Play on the grid division this tick is for, not "now".
                player->playAt(masterClock.getTickTime());
                if (activeLooper && addLooper) {
                    bool success = looperManager.addAudioLooper(keypadIDString, player, loopDuration);
                    if (verbose) {
                        printf("   AudioManager::audioPlaybackTask::addLooper success: %d.\n", success);
                    }
                }
            } else {
                if (verbose) {
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
