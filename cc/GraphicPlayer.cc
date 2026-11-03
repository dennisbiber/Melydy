// GraphicPlayer.cc

#include "GraphicPlayer.h"
#include "iostream"
#include "iostream"
#include <string>
#include <mutex>

GraphicPlayer::GraphicPlayer(const YAML::Node& windowConfig, 
    bool verbose, bool superVerbose, bool timeVerbose) : 
    verbose(verbose), superVerbose(superVerbose), timeVerbose(timeVerbose),
    animationLoopRunning(false), fontSize(24),
    window(nullptr), renderer(nullptr), your_font(nullptr),
    labelTexture(nullptr), labelWidth(0), labelHeight(0) {
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

    // Create the renderer. Fall back to software rendering when there's no
    // GPU driver (VNC, some Pi setups, headless test runs).
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::cerr << "         ---GraphicPlayer::GraphicPlayer::Failed to create SDL renderer: " << SDL_GetError() << std::endl;
        TTF_Quit(); // Quit SDL_ttf before SDL_Quit
        SDL_DestroyWindow(window); // Destroy the GraphicPlayer if the renderer creation failed
        window = nullptr;
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
    if (labelTexture != nullptr) {
        SDL_DestroyTexture(labelTexture);
    }
    if (your_font != nullptr) {
        TTF_CloseFont(your_font);
    }
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

// Draw one frame. Call from the main thread only (same thread that created
// the window). The label texture is only rebuilt when the text changes.
void GraphicPlayer::renderFrame(const std::string& label) {
    if (renderer == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, 18, 18, 24, 255);
    SDL_RenderClear(renderer);

    if (your_font != nullptr && label != currentLabel) {
        if (labelTexture != nullptr) {
            SDL_DestroyTexture(labelTexture);
            labelTexture = nullptr;
        }
        currentLabel = label;
        SDL_Color color = {235, 235, 235, 255};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(your_font, label.c_str(), color);
        if (surface != nullptr) {
            labelTexture = SDL_CreateTextureFromSurface(renderer, surface);
            labelWidth = surface->w;
            labelHeight = surface->h;
            SDL_FreeSurface(surface);
        }
    }
    if (labelTexture != nullptr) {
        int windowWidth = 0, windowHeight = 0;
        SDL_GetRendererOutputSize(renderer, &windowWidth, &windowHeight);
        SDL_Rect dst = {(windowWidth - labelWidth) / 2, (windowHeight - labelHeight) / 2, labelWidth, labelHeight};
        SDL_RenderCopy(renderer, labelTexture, nullptr, &dst);
    }
    SDL_RenderPresent(renderer);
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