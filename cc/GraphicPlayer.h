// GraphicPlayer.h

#ifndef GRAPHIC_PLAYER_H
#define GRAPHIC_PLAYER_H

#include "Structures.h"
#ifdef _WIN32
#include <SDL.h> // Include path for GRAPHIC_PLAYER_H
#include <SDL_ttf.h>
#else
#include <SDL2/SDL.h> // Include path for Linux
#include <SDL2/SDL_ttf.h>
#endif
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>

class GraphicPlayer {
    public:
        GraphicPlayer(const YAML::Node& windowConfig, 
            bool verbose, bool superVerbose, bool timeVerbose);
        ~GraphicPlayer();
        void startAnimationLoop();
        void stopAnimationLoopRunning();
        bool isAnimationLoopRunning();

    private:
        SDL_Window* window;
        SDL_Renderer* renderer;
        TTF_Font* your_font;
        bool verbose;
        bool superVerbose;
        bool timeVerbose;
        bool animationLoopRunning;
        // int g1Width;
        // int g1Height;
        // int g2Width;
        // int g2Height;
        // int p2Width;
        // int p2Height;
        // int noteWidth;
        // int noteHeight;
        int fontSize;
        std::string font;
};


#endif // SDL_GRAPHIC_END