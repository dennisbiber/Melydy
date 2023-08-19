#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "Keyboard Mapping",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        0
    );

    SDL_Event event;

    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Get the current state of all keys
        const Uint8* currentKeyState = SDL_GetKeyboardState(NULL);

        // Print the mapping of SDL_SCANCODE values and their corresponding keys
        for (int scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {
            if (currentKeyState[scancode]) {
                const char* keyName = SDL_GetScancodeName(SDL_Scancode(scancode));
                std::cout << "SDL_SCANCODE " << scancode << ": " << keyName << std::endl;
            }
        }

        SDL_Delay(100); // To prevent the loop from running too fast
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

