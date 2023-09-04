// Manager.h
#ifndef MANAGER_H
#define MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include "AudioManager.h"
#include "GraphicManager.h"
#include "KeyboardEvent.h"
#include "LooperManager.h"
#include "MasterClock.h"
#include "Structures.h"
// #include "Window.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>

typedef float Sample;

class Manager {
    public:
        Manager(MasterClock& mc,
            const YAML::Node& verbosity,
            const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
            const YAML::Node& notesConfig, const YAML::Node& windowConfig,
            const YAML::Node& audioMixerConfig);
        ~Manager();

        void joinManagerThread();
        void updateStates();
        void setFunction();
        
    private:
        void scheduleupdateStates();
        void scheduleAudioLooperTask();
        void scheduleAudioPlaybackTask();
        void startKeyboardThread();
        void startAnimationThread();
        MasterClock& masterClock;
        KeyboardEvent keyboardEvent;
        LooperManager looperManager;
        AudioManager audioManager;
        GraphicManager graphicManager;

        // functions
        void setNotesConfig();

        // variables
        const YAML::Node& verbosity;
        bool verbose;
        bool superVerbose;
        bool timeVerbose;
        int mixerBufferSize;
        std::string currentFunction;
        std::mutex managerThreadMutex;
        std::condition_variable managerThreadCV;
        void audioPlaybackHandler();
        
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
        const YAML::Node& notesConfig;
};

#endif // MANAGER_H