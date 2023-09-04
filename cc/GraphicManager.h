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
        GraphicManager(const YAML::Node& graphicVerbosity, bool superVerbose, bool timeVerbose,
            MasterClock& mc, const YAML::Node& windowConfig);
        ~GraphicManager();
        void startAnimationWindow();
        void stopAnimationWindow();

    private:
        const YAML::Node& verbosity;
        GraphicProcessor graphicProcessor;
        GraphicPlayer graphicPlayer;
        MasterClock& masterClock;
        bool verbose;
        bool superVerbose;
        bool timeVerbose;
        std::thread windowThread;
};

#endif // AUDIO_MANAGER_H
