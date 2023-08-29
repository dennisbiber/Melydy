#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "AudioPlayer.h"
#include "AudioProcessor.h"
#include "KeyboardEvent.h"
#include "LooperManager.h"
#include "MasterClock.h"
#include "Structures.h"
#include <algorithm>
#include <cstddef>
#include <chrono>

typedef float Sample;
using Duration = std::chrono::high_resolution_clock::duration;

class AudioManager {
    public:
        AudioManager(bool verbose, bool superVerbose, bool audioPlayerVerbose, bool audioProcessorVerbose, 
            MasterClock& mc, KeyboardEvent& kb, LooperManager& lm,
            const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
            const YAML::Node& audioMixerConfig,
            std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex);
        ~AudioManager();
        bool addAudioPlayer(const char* filepath, NoteConfiguration config);
        bool removeAudioPlayer(SDL_Scancode keycode);
        void schedulePlayback();
        void unschedulePlayback();
        void setCurrentFunction(std::string function);
        void setKeypadReady(bool stateUpdate);

    private:
        void audioPlaybackTask();
        AudioPlayer* getAudioPlayer(SDL_Scancode keycode);

        MasterClock& masterClock;
        KeyboardEvent& keyboardEvent;
        LooperManager& looperManager;
        AudioProcessor audioProcessor;
        AudioPlayerMapThreadings audioPlayermapThreadings;

        std::unordered_map<std::string, std::pair<std::string, AudioPlayer*>> playerMap;
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
        std::vector<NoteConfiguration> noteConfigurations;
        std::shared_ptr<ManagerThreadings> sharedManagerThreadings;
        std::atomic<bool> stopFlag = ATOMIC_VAR_INIT(false);
        bool verbose;
        bool superVerbose;
        bool audioPlayerVerbose;
        bool audioProcessorVerbose;
        bool runAudioPlaybackThread;
        bool addLooper;
        double bpm;
        Duration beatDurationAsDuration;
        double beatDivisions;
        std::string currentFunction;
        std::thread audioPlaybackThread;
        std::mutex noteConfigurationsMutex;
        std::condition_variable& managerThreadCV;
        std::mutex& managerThreadMutex;
        std::mutex playerMapMutex;
};

#endif // AUDIO_MANAGER_H
