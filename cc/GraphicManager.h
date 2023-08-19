#ifndef GRAPHIC_MANAGER_H
#define GRAPHIC_MANAGER_H

#include "GraphicProcessor.h"
#include "GraphicPlayer.h"
#include "MasterClock.h"
#include <cstddef>
#include <chrono>

typedef float Sample;

class GraphicManager {
    public:
        GraphicManager(bool verbose, bool superVerbose, bool graphicPlayerVerbose, 
            bool graphicProcessorVerbose, bool timeVerbose,
            MasterClock& mc, const YAML::Node& windowConfig,
            std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex);
        ~GraphicManager();
        void startAnimationWindow();
        void stopAnimationWindow();

    private:
        GraphicProcessor graphicProcessor;
        GraphicPlayer graphicPlayer;
        MasterClock& masterClock;
        bool verbose;
        bool superVerbose;
        bool timeVerbose;
        bool graphicPlayerVerbose;
        bool graphicProcessorVerbose;
        std::thread windowThread;
        std::condition_variable& managerThreadCV;
        std::mutex& managerThreadMutex;
};

#endif // AUDIO_MANAGER_H
