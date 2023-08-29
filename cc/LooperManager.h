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
        bool verbose, bool superVerbose, bool timeVerbose,
        bool audioLooperVerbose, bool graphicLooperVerbose,
        std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex);
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

        bool verbose; 
        bool superVerbose;
        bool timeVerbose;
        bool audioLooperVerbose;
        bool graphicLooperVerbose;
        bool addLooper;
        bool removeLooper;
        bool runAudioLooperThread;

        std::thread audioLooperThread;
        std::mutex audioLoopersMutex;
        std::condition_variable& managerThreadCV;
        std::mutex& managerThreadMutex;
        //std::unordered_map<std::string, std::pair<std::string, std::pair<bool, std::shared_ptr<AudioLooper>>>> audioLoopers;
        std::unordered_multimap<std::string, AudioLooperInfo> audioLoopers;
        std::unordered_multimap<std::string, std::shared_ptr<AudioLooper>> keypadIDToLoopers;
        const std::unordered_map<std::string, std::pair<bool*, double>>& stringBoolPairs;
};

#endif // LOOPER_MANAGER_H