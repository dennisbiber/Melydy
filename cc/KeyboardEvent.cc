#include "KeyboardEvent.h"
#include <thread>
#include <iostream>
#include <algorithm> 
#include <unordered_set>
#include <array>

KeyboardEvent::KeyboardEvent(MasterClock& mc, bool vb, bool timeVerbose, bool superVerbose)
    : masterClock(mc), verbose(vb), timeVerbose(timeVerbose), superVerbose(superVerbose), 
    currentKeyState(SDL_GetKeyboardState(NULL)),
    logging(false), newFunction(false), activeFNIndex(9), currentFunction("fn10"),
    addLooper(false), removeLooper(false), quit(false) {
    initPressedKeysMap();
    initKeypadPressedKeysMap();
    initKeypadCtrlPressedKeysMap();
    initFunctionPressedKeysMap();
    if (verbose) {
        printf("       KeyboardEvent::KeyboardEvent::Constructed.\n");
    }
}

KeyboardEvent::~KeyboardEvent(){
}
// INIT SECTION
// #################################################################################################
void KeyboardEvent::initPressedKeysMap() {
    if (verbose) {
        printf("       KeyboardEvent::initPressedKeysMap::Entered.\n");
    }
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        SDL_Scancode scancode = static_cast<SDL_Scancode>(i);
        if (!isKeypadControlKey(scancode) && !isKeypadKey(scancode) && !isFunctionControlKey(scancode))
        pressedKeys[scancode] = false;
    }
}

void KeyboardEvent::initKeypadPressedKeysMap() {
    if (verbose) {
        printf("       KeyboardEvent::initKeypadPressedKeysMap::Entered.\n");
    }
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        SDL_Scancode scancode = static_cast<SDL_Scancode>(i);
        if (isKeypadKey(scancode)) {
            keypadPressedKeys[scancode] = false;
        } 
    }
}

void KeyboardEvent::initKeypadCtrlPressedKeysMap() {
    if (verbose) {
        printf("       KeyboardEvent::initKeypadCtrlPressedKeysMap::Entered.\n");
    }
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        SDL_Scancode scancode = static_cast<SDL_Scancode>(i);
        if (isKeypadControlKey(scancode)) {
            keypadCtrlPressedKeys[scancode] = false;
        } 
    }
}

void KeyboardEvent::initFunctionPressedKeysMap() {
    if (verbose) {
        printf("       KeyboardEvent::initFunctionPressedKeysMap::Entered.\n");
    }
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        SDL_Scancode scancode = static_cast<SDL_Scancode>(i);
        if (isFunctionControlKey(scancode)) {
            functionPressedKeys[scancode] = false;
        } 
    }
}
// Thread Managment SECTION
// #################################################################################################
void KeyboardEvent::startHandlingEvents() {
    if (verbose) {
        printf("      KeyboardEvent::startHandlingEvents::Starting Keyboard Thread.\n");
    }
    keyboardThread = std::thread(&KeyboardEvent::handleKeyboardEvent, this);    
}

void KeyboardEvent::stopHandlingEvents() {
    if (verbose) {
        printf("       KeyboardEvent::stopHandlingEvents::Signaling to Stop Keyboard Thread.\n");
    }
    stopFlag.store(true);

    if (keyboardThread.joinable()) {
        keyboardThread.join();  // Wait for the thread to finish
    }
}
// Getter/Setter Functions SECTION
// #################################################################################################
bool KeyboardEvent::getKeypadStates(int keypadNumber) {
    if (keypadNumber >= 0 && keypadNumber <= 8) {
        std::lock_guard<std::mutex> lock(isKeypadLockedMutex);
        if (verbose && superVerbose && isKPLocked[keypadNumber]) {
            printf("       KeyboardEvent::getKeypadStates::KeypadNumber: %d.\n", keypadNumber);
            printf("       KeyboardEvent::getKeypadStates::KeypadNumber: %d.\n", isKPLocked[keypadNumber]);
        }
        return isKPLocked[keypadNumber];
    } 
    if (keypadNumber == -1) {
        return addLooper;
    } 
    if (keypadNumber == -2) {
        return removeLooper;
    }
    return false;
}

bool KeyboardEvent::isScancodeDataEmpty() {
    std::lock_guard<std::mutex> lock(scancodeDataMutex);
    if (verbose) {
        printf("          KeyboardEvent::isScancodeDataEmpty: %d.\n", scancodeData.empty());
    }
    return scancodeData.empty();
}

void KeyboardEvent::functionFetchReset() {
    std::lock_guard<std::mutex> lock(newFunctionMutex);
    newFunction = false;
}

std::string KeyboardEvent::getFunctionState() {
    return currentFunction;
}

void KeyboardEvent::setScancodeData(SDL_Scancode scancode) {
    std::lock_guard<std::mutex> lock(scancodeDataMutex);
    if (scancodeData.size() < 4) {
        scancodeData.insert(scancode);
    }
}

const std::unordered_set<SDL_Scancode>& KeyboardEvent::getScancodeData() {
    std::lock_guard<std::mutex> lock(scancodeDataMutex);
    if (verbose) {
        printf("       KeyboardEvent::getScancodeData::Codes ");
        for (const int& code : scancodeData) {
            printf("%d ", code);
        }
        printf("\n");
    }
    return scancodeData;
}
// Validator Functions SECTION
// #################################################################################################
bool KeyboardEvent::isKeypadKey(SDL_Scancode scancode) {
    // Return true if the scancode corresponds to a keypad key
    // Modify this as needed to include the desired keypad keys
    return (scancode == SDL_SCANCODE_KP_0 ||
            scancode == SDL_SCANCODE_KP_1 ||
            scancode == SDL_SCANCODE_KP_2 ||
            scancode == SDL_SCANCODE_KP_3 ||
            scancode == SDL_SCANCODE_KP_4 ||
            scancode == SDL_SCANCODE_KP_5 ||
            scancode == SDL_SCANCODE_KP_6 ||
            scancode == SDL_SCANCODE_KP_7 ||
            scancode == SDL_SCANCODE_KP_8 ||
            scancode == SDL_SCANCODE_KP_9);
}

bool KeyboardEvent::isKeypadControlKey(SDL_Scancode scancode) {
    return (scancode == SDL_SCANCODE_KP_PLUS ||
            scancode == SDL_SCANCODE_KP_MINUS ||
            scancode == SDL_SCANCODE_KP_ENTER);
}

bool KeyboardEvent::isFunctionControlKey(SDL_Scancode scancode) {
    return (scancode == SDL_SCANCODE_F1 ||
            scancode == SDL_SCANCODE_F2 ||
            scancode == SDL_SCANCODE_F3 ||
            scancode == SDL_SCANCODE_F4 ||
            scancode == SDL_SCANCODE_F5 ||
            scancode == SDL_SCANCODE_F6 ||
            scancode == SDL_SCANCODE_F7 ||
            scancode == SDL_SCANCODE_F8 ||
            scancode == SDL_SCANCODE_F9 ||
            scancode == SDL_SCANCODE_F10 ||
            scancode == SDL_SCANCODE_F11 ||
            scancode == SDL_SCANCODE_F12);
}

// Handler Functions SECTION
// #################################################################################################
void KeyboardEvent::handleKeypadKey(SDL_Scancode scancode) {
    // Handle the keypad key press here{
    if (verbose) {
        std::cout << "          KeyboardEvent::Keypad scancode " << scancode << std::endl;
    }
    if (scancode >= 89 && scancode <= 97) {
        if (verbose && superVerbose) {
            printf("               KeyboardEvent::handleKeypadKey::Scancode: %d.\n", scancode);
            printf("               KeyboardEvent::handleKeypadKey::State: %d.\n", keypadPressedKeys[scancode]);
        }
        std::lock_guard<std::mutex> lock(isKeypadLockedMutex);
        isKPLocked[scancode - 89] = keypadPressedKeys[scancode];
    }
}

void KeyboardEvent::handleFunctionKey(SDL_Scancode scancode) {
    if (scancode >= 58 && scancode <= 69) {
        activeFNIndex = scancode - 58;
        if (activeFNIndex <= 8) {
            currentFunction = "fn0" + std::to_string(activeFNIndex + 1);
        } else if (activeFNIndex >= 9 && activeFNIndex < 12) {
            currentFunction = "fn" + std::to_string(activeFNIndex + 1);
        }
        std::lock_guard<std::mutex> lock(newFunctionMutex);
        newFunction = true;
        if (verbose) {
            printf("          KeyboardEvent::handleFunctionKeys::Setting New Function: %s.\n", currentFunction.c_str());
            for (const auto& pair : pressedKeys) {
                printf("         -Key: %d, Value: %d\n", pair.first, pair.second);
            }
        }
    }
}

// SDL_SCANCODE 58: F1, SDL_SCANCODE 59: F2, SDL_SCANCODE 60: F3
// SDL_SCANCODE 61: F4, SDL_SCANCODE 62: F5, SDL_SCANCODE 63: F6
// SDL_SCANCODE 64: F7, SDL_SCANCODE 65: F8, SDL_SCANCODE 66: F9
// SDL_SCANCODE 67: F10, SDL_SCANCODE 68: F11, SDL_SCANCODE 69: F12

void KeyboardEvent::handleKeypadControls(SDL_Scancode scancode) {
    if (verbose) {
        printf("       KeyboardEvent::Keyboard Thread::Inside Keypad Control Handler.\n");
    }
    if (scancode == 87){
        addLooper = keypadCtrlPressedKeys[scancode];
    } else if (scancode == 86) {
        removeLooper = keypadCtrlPressedKeys[scancode];
    }
}

void KeyboardEvent::handleAlphaNumericKeyDown(SDL_Scancode scancode) {
    if (verbose) {
        printf("       KeyboardEvent::Keyboard Thread::Inside Handler.\n");
    }
    if (pressedKeys[scancode]) {
        if (verbose) {
            printf("       KeyboardEvent::handleKeyboardEvent::Codes to play construct: Code: %d\n", scancode);
        }
        setScancodeData(scancode);
    }
}

void KeyboardEvent::clearScancodeData() {
    std::lock_guard<std::mutex> lock(scancodeDataMutex);
    if (verbose && superVerbose) {
        printf("       KeyboardEvent::clearScancodeData.\n");
    }
    scancodeData.clear();
}
// THREAD SECTION
// #################################################################################################
void KeyboardEvent::handleKeyboardEvent() {
    bool superVerbose = false;
    if (verbose) {
        printf("       KeyboardEvent::handleKeyboardEvent::Initialized.\n");
    }

    quit = false;
    SDL_Event event;
    while (!quit || !stopFlag) {
        if (verbose && timeVerbose) {
            masterClock.startTimer("KeyboardEvent", true);
        }

        // while (SDL_PollEvent(&event)) {
        SDL_WaitEvent(&event);
        SDL_Scancode theCode = event.key.keysym.scancode;
        switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                if (verbose && superVerbose) {
                    printf("       KeyboardEvent::handleKeyboardEvent::Event Loop.\n");
                }

                // Check if the scancode is a key in the pressedKeys map
                if (event.key.repeat == 0) {
                    std::lock_guard<std::mutex> lock(pressedKeysMutex);
                    if (pressedKeys.find(theCode) != pressedKeys.end()) {
                        if (!pressedKeys[theCode]) {
                            pressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                            handleAlphaNumericKeyDown(theCode);
                        }
                    } else if (keypadPressedKeys.find(theCode) != keypadPressedKeys.end()) {
                        if (!keypadPressedKeys[theCode]) {
                            keypadPressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                            handleKeypadKey(theCode);
                        }
                    } else if (keypadCtrlPressedKeys.find(theCode) != keypadCtrlPressedKeys.end()) {
                        if (!keypadCtrlPressedKeys[theCode]) {
                            keypadCtrlPressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                            handleKeypadControls(theCode);
                        }
                    } else if (functionPressedKeys.find(theCode) != functionPressedKeys.end()) {
                        if (!functionPressedKeys[theCode]) {
                            functionPressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                            handleFunctionKey(theCode);
                        }
                    }
                    break;
                }
            case SDL_KEYUP:
                // std::cout << "Keypad scancode " << theCode << std::endl;
                if (pressedKeys.find(theCode) != pressedKeys.end()) {
                    std::lock_guard<std::mutex> lock(pressedKeysMutex);
                    if (pressedKeys[theCode]) {
                        pressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                    }
                } else if (keypadPressedKeys.find(theCode) != keypadPressedKeys.end()) {
                    if (keypadPressedKeys[theCode]) {
                        keypadPressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                        handleKeypadKey(theCode);
                    }
                } else if (keypadCtrlPressedKeys.find(theCode) != keypadCtrlPressedKeys.end()) {
                    if (keypadCtrlPressedKeys[theCode]) {
                        keypadCtrlPressedKeys[theCode]  = currentKeyState[theCode] == SDL_PRESSED;;
                        handleKeypadControls(theCode);
                    }
                } else if (functionPressedKeys.find(theCode) != functionPressedKeys.end()) {
                    if (functionPressedKeys[theCode]) {
                        functionPressedKeys[theCode] = currentKeyState[theCode] == SDL_PRESSED;;
                        handleFunctionKey(theCode);
                    }
                }
                break;
            // Handle other event types if needed
            default:
                break;
        }
        // }

        if (verbose && timeVerbose) {
            masterClock.startTimer("KeyboardEvent", false);
            printf("        EventHandler Loop Complete::%s\n", masterClock.getDurationString("KeyboardEvent").c_str());
        }
    }
    printf("      KeyboardEvent::Event Loop Exited.\n");
}


// SDL_SCANCODE 89: Keypad 1
// SDL_SCANCODE 90: Keypad 2
// SDL_SCANCODE 91: Keypad 3
// SDL_SCANCODE 92: Keypad 4
// SDL_SCANCODE 93: Keypad 5
// SDL_SCANCODE 94: Keypad 6
// SDL_SCANCODE 95: Keypad 7
// SDL_SCANCODE 96: Keypad 8
// SDL_SCANCODE 97: Keypad 9
// SDL_SCANCODE 98: Keypad 0
// SDL_SCANCODE 99: Keypad .
// SDL_SCANCODE 88: Keypad Enter
// SDL_SCANCODE 87: Keypad +
// SDL_SCANCODE 86: Keypad -
