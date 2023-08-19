#include "GraphicManager.h"
#include <cstdio>
#include <cmath>

GraphicManager::GraphicManager(bool verbose, bool superVerbose, bool graphicPlayerVerbose, 
    bool graphicProcessorVerbose, bool timeVerbose,
    MasterClock& mc, const YAML::Node& windowConfig,
    std::condition_variable& managerThreadCV, std::mutex& managerThreadMutex) : 
    verbose(verbose), graphicPlayerVerbose(graphicPlayerVerbose),
    superVerbose(superVerbose), timeVerbose(timeVerbose), 
    graphicProcessorVerbose(graphicProcessorVerbose), 
    graphicProcessor(graphicProcessorVerbose), 
    graphicPlayer(windowConfig, graphicPlayerVerbose, superVerbose, timeVerbose),
    masterClock(mc), managerThreadCV(managerThreadCV),
    managerThreadMutex(managerThreadMutex) {
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