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
        AudioManager(MasterClock& mc, KeyboardEvent& kb, LooperManager& lm,
            const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
            const YAML::Node& audioVerbosity, const YAML::Node& audioMixerConfig,
            bool sV);
        ~AudioManager();
        bool addAudioPlayer(const char* filepath, NoteConfiguration config);
        bool removeAudioPlayer(SDL_Scancode keycode);
        void schedulePlayback();
        void unschedulePlayback();
        void setCurrentFunction(std::string function);
        void setKeypadReady(bool stateUpdate);

    private:
        // FUNCTIONS
        void audioPlaybackTask();
        AudioPlayer* getAudioPlayer(SDL_Scancode keycode);

        // OBJECTS
        MasterClock& masterClock;
        KeyboardEvent& keyboardEvent;
        LooperManager& looperManager;
        AudioProcessor audioProcessor;
        AudioPlayerMapThreadings audioPlayermapThreadings;

        // VARIABLES
        bool audioPlayerVerbose;
        std::unordered_map<std::string, std::pair<std::string, AudioPlayer*>> playerMap;
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
        std::vector<NoteConfiguration> noteConfigurations;
        bool verbose;
        bool superVerbose;
        bool runAudioPlaybackThread;
        bool addLooper;
        double bpm;
        Duration beatDurationAsDuration;
        double beatDivisions;
        std::string currentFunction;
        std::thread audioPlaybackThread;
        std::mutex playerMapMutex;
};

#endif // AUDIO_MANAGER_H
