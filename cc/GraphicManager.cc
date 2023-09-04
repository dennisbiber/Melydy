#include "GraphicManager.h"
#include <cstdio>
#include <cmath>

GraphicManager::GraphicManager(const YAML::Node& graphicVerbosity, bool superVerbose, bool timeVerbose,
    MasterClock& mc, const YAML::Node& windowConfig) :
    verbosity(graphicVerbosity),
    verbose(verbosity["graphicManagerVerbose"].as<bool>()),
    superVerbose(superVerbose), timeVerbose(timeVerbose), 
    graphicProcessor(verbosity["graphicProcessorVerbose"].as<bool>()), 
    graphicPlayer(windowConfig, verbosity["graphicPlayerVerbose"].as<bool>(), superVerbose, timeVerbose),
    masterClock(mc) {
}

GraphicManager::~GraphicManager() {
    stopAnimationWindow();
}

void GraphicManager::startAnimationWindow() {
    windowThread = std::thread([this]() {
        try {
            graphicPlayer.startAnimationLoop();
        } catch (const std::exception& e) {
            // Handle any exceptions that might occur during the animation loop
            printf("Exception in GraphicPlayer: %s\n", e.what());
        }
    });
}
void GraphicManager::stopAnimationWindow() {
    if (verbose) {
        printf("    GraphicManager::stopAnimationWindow::Signaling to Stop Keyboard Thread.\n");
    }

    if (graphicPlayer.isAnimationLoopRunning()) {
        graphicPlayer.stopAnimationLoopRunning();
    }

    if (windowThread.joinable()) {
        windowThread.join(); // Wait for the thread to finish
    }
}