// GraphicPlayer.cc

#include "GraphicPlayer.h"
#include "iostream"
#include "iostream"
#include <string>
#include <mutex>

GraphicPlayer::GraphicPlayer(const YAML::Node& windowConfig, 
    bool verbose, bool superVerbose, bool timeVerbose) : 
    verbose(verbose), superVerbose(superVerbose), timeVerbose(timeVerbose),
    animationLoopRunning(false), fontSize(24) {
    // int gauge1Width = windowConfig["g1Width"].as<int>();
    // int gauge1Height = windowConfig["g1Height"].as<int>();
    // int gauge2Width = windowConfig["g2Width"].as<int>();
    // int gauge2Height = windowConfig["g2Height"].as<int>();
    // int pulse2Width = windowConfig["p2Width"].as<int>();
    // int pulse2Height = windowConfig["p2Height"].as<int>();
    // int noteWidth = windowConfig["noteWidth"].as<int>();
    // int noteHeight = windowConfig["noteHeight"].as<int>();
    std::string font = windowConfig["font"].as<std::string>();
    int fontSize = windowConfig["fontSize"].as<int>();
    if (font.empty()) {
        std::cerr << "---Font path is empty. Check your configuration." << std::endl;
        // Handle the error, e.g., throw an exception or return an error code
    }
    const char* title = "SAMPLER_DRUMPAD";
    int x = SDL_WINDOWPOS_UNDEFINED;
    int y = SDL_WINDOWPOS_UNDEFINED;
    int width = 800;
    int height = 600;
    Uint32 flags = SDL_WINDOW_SHOWN;
    // Create an SDL window
     if (TTF_Init() == -1) {
        std::cerr << "         ---GraphicPlayer::GraphicPlayer::Failed to initialize SDL_ttf: " << TTF_GetError() << std::endl;
        // Handle the error, e.g., throw an exception or exit the program
    } else {
        if (verbose) {
            printf("         GraphicPlayer::GraphicPlayer::TTF Initialized.\n");
        }
    }

    SDL_ShowCursor(SDL_DISABLE);

    // Get the base path of the executable
    char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        std::cerr << "         ---GraphicPlayer::GraphicPlayer::Failed to get the base path: " << SDL_GetError() << std::endl;
        // Handle the error, e.g., throw an exception or exit the program
        TTF_Quit(); // Quit SDL_ttf before SDL_Quit
        return;
    }
    this->your_font = TTF_OpenFont(font.c_str(), fontSize);
    if (!your_font) {
        std::cerr << "         ---GraphicPlayer::GraphicPlayer::Failed to load font: " << TTF_GetError() << std::endl;
        // Handle the error, e.g., throw an exception or exit the program
    } else {
        if (verbose) {
            printf("         GraphicPlayer::GraphicPlayer::Loaded Font.\n");
        }
    }

    // Create the GraphicPlayer
    window = SDL_CreateWindow(title, x, y, width, height, flags);
    if (window == NULL) {
        fprintf(stderr, "         ---GraphicPlayer::GraphicPlayer::Failed to create SDL GraphicPlayer: %s\n", SDL_GetError());
        // You can throw an exception here or handle the error in any way you prefer
    }

    // Create the renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "         ---GraphicPlayer::GraphicPlayer::Failed to create SDL renderer: " << SDL_GetError() << std::endl;
        TTF_Quit(); // Quit SDL_ttf before SDL_Quit
        SDL_DestroyWindow(window); // Destroy the GraphicPlayer if the renderer creation failed
        // You can throw an exception here or handle the error in any way you prefer
    }
    // Calculate the position to center the text in the GraphicPlayer
    SDL_GetWindowSize(window, &width, &height);
    if (verbose) {
        printf("         GraphicPlayer::GraphicPlayer::Constructed.\n");
    }
}

GraphicPlayer::~GraphicPlayer() {
    if (verbose) {
        printf("         GraphicPlayer::~GraphicPlayer::Entered.\n");
    }
    if (animationLoopRunning) {
        stopAnimationLoopRunning();
    }
    TTF_CloseFont(your_font);
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    // Quit SDL_ttf
    TTF_Quit();
}

bool GraphicPlayer::isAnimationLoopRunning() {
    return animationLoopRunning;
}

void GraphicPlayer::stopAnimationLoopRunning() {
    animationLoopRunning = false;
}

void GraphicPlayer::startAnimationLoop() {
    animationLoopRunning = true;
    Uint32 lastFrameTime = SDL_GetTicks();
    const int frameRate = 60;
    const int frameDelay = 1000 / frameRate;

    while(animationLoopRunning) {
        if (verbose) {
            printf("            WindowLoop.\n");
        }
        SDL_RenderPresent(renderer);
        Uint32 currentFrameTime = SDL_GetTicks();
        Uint32 frameTimeDiff = currentFrameTime - lastFrameTime;

        if (frameTimeDiff < frameDelay) {
            SDL_Delay(frameDelay - frameTimeDiff);
        }

        lastFrameTime = currentFrameTime;
    }
}