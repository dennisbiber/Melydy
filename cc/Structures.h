// Structures.h
#ifndef STRUCTURES_H
#define STRUCTURES_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#else
#include <SDL2/SDL.h> // Include path for Linux
#endif
// #include "ThreadPool.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

#include "StreamClock.h"

struct NoteConfiguration {
    std::string noteName;
    SDL_Scancode keycode;
    std::string functionAssignment;
};

struct ManagerThreadings {
    std::thread managerThread;
    std::mutex managerThreadMutex;
    std::condition_variable managerThreadCV;
};

struct AudioPlayerMapThreadings {
    std::mutex playerMapMutex;
};

#endif // STRUCTURES_H