// Structures.h
#ifndef STRUCTURES_H
#define STRUCTURES_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h> // Include path for Linux
#include <SDL2/SDL_mixer.h>
#endif
// #include "ThreadPool.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

using Duration = std::chrono::high_resolution_clock::duration;
using TimePoint = std::chrono::high_resolution_clock::time_point;

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