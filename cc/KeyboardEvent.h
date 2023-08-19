#ifndef KEYBOARDEVENT_H
#define KEYBOARDEVENT_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#else
#include <SDL2/SDL.h> // Include path for Linux
#endif
#include "MasterClock.h"
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <mutex>
#include <array>

class KeyboardEvent {
public:
    KeyboardEvent(MasterClock& mc, bool vb, bool timeVerbose, bool superVerbose);

    ~KeyboardEvent();

    void startHandlingEvents();

    void clearScancodeData();
    bool getKeypadStates(int keypadNumber);

    bool isScancodeDataEmpty();
    const std::unordered_set<SDL_Scancode>& getScancodeData();
    void handleKeyboardEvent();
    void stopHandlingEvents();
    void functionFetchReset();
    std::string getFunctionState();

private:
    bool verbose;
    bool timeVerbose;
    bool superVerbose;
    MasterClock& masterClock;
    std::unordered_map<SDL_Scancode, bool> pressedKeys;
    std::unordered_map<SDL_Scancode, bool> keypadPressedKeys;
    std::unordered_map<SDL_Scancode, bool> keypadCtrlPressedKeys;
    std::unordered_map<SDL_Scancode, bool> functionPressedKeys;
    std::unordered_set<SDL_Scancode> scancodeData;
    std::mutex keypadPressedKeysMutex;
    std::mutex isKeypadLockedMutex;
    std::mutex newFunctionMutex;
    std::mutex pressedKeysMutex;
    std::mutex keyboardMutex;
    std::mutex scancodeDataMutex;
    std::condition_variable keyboardCV;
    std::atomic<bool> stopFlag = ATOMIC_VAR_INIT(false);
    std::thread keyboardThread;
    bool isKPLocked[9] = {false};
    bool keypadLockStates[10] = {false};
    bool addLooper;
    bool removeLooper;
    bool logging;
    bool newFunction;
    bool quit;
    int activeFNIndex;
    std::string currentFunction;

    void handleKeypadKey(SDL_Scancode scancode);
    void handleAlphaNumericKeyDown(SDL_Scancode scancode);
    void handleKeypadControls(SDL_Scancode scancode);
    void handleFunctionKey(SDL_Scancode scancode);
    bool isKeypadKey(SDL_Scancode scancode);
    bool isKeypadControlKey(SDL_Scancode scancode);
    bool isFunctionControlKey(SDL_Scancode scancode);
    void setScancodeData(SDL_Scancode scancode);
    void initPressedKeysMap();
    void initKeypadPressedKeysMap();
    void initKeypadCtrlPressedKeysMap();
    void initFunctionPressedKeysMap();

    const Uint8* currentKeyState;
};

#endif // KEYBOARDEVENT_H
