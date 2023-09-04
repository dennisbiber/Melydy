// LooperManager.h
#ifndef LOOPER_MANAGER_H
#define LOOPER_MANAGER_H

#include "AudioLooper.h"
#include "AudioPlayer.h"
#include "KeyboardEvent.h"
#include "MasterClock.h"
#include "Structures.h"
#include <vector>

struct AudioLooperInfo {
    std::string keypadIDStr;
    bool isLooping;
    std::shared_ptr<AudioLooper> audioLooper;
};

class LooperManager {
    public:
        LooperManager(MasterClock& mc, KeyboardEvent& kb,
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs,
        const YAML::Node& verbosity, bool superVerbose, bool timeVerbose);
        ~LooperManager();
        void audioLooperTask();
        bool addAudioLooper(const std::string& keypadIDStr, AudioPlayer* player, double loopDuration);
        void loopSetter();
        void scheduleLooperTask();
        void setRemoveLooper(bool removeState);

    private:
        void removeAudioLoopers(const std::string& keypadIDStr);
        MasterClock& masterClock;
        KeyboardEvent& keyboardEvent;

        const YAML::Node& verbosity;
        bool verbose; 
        bool superVerbose;
        bool timeVerbose;
        bool audioLooperVerbose;
        bool graphicLooperVerbose;
        bool addLooper;
        bool removeLooper;
        bool runAudioLooperThread;

        std::unordered_multimap<std::string, AudioLooperInfo> audioLoopers;
        std::unordered_multimap<std::string, std::shared_ptr<AudioLooper>> keypadIDToLoopers;
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
};

#endif // LOOPER_MANAGER_H