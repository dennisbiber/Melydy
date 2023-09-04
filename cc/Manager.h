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
            const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
            const YAML::Node& verbosity,
            const YAML::Node& notesConfig, const YAML::Node& windowConfig,
            const YAML::Node& audioMixerConfig,
            bool sV, bool tV);
        ~Manager();

        void joinManagerThread();
        void updateStates();
        void setFunction();
        
    private:
        // functions
        void setNotesConfig();
        void scheduleupdateStates();
        void scheduleAudioLooperTask();
        void scheduleAudioPlaybackTask();
        void startKeyboardThread();
        void startAnimationThread();

        // Objects
        MasterClock& masterClock;
        KeyboardEvent keyboardEvent;
        LooperManager looperManager;
        GraphicManager graphicManager;
        AudioManager audioManager;

        // variables
        bool verbose;
        bool superVerbose;
        bool timeVerbose;
        int mixerBufferSize;
        std::string currentFunction;
        
        // Structures
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
        const YAML::Node& notesConfig;
};

#endif // MANAGER_H