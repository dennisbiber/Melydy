// AudioLooper.h
#ifndef AUDIO_LOOPER_H
#define AUDIO_LOOPER_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h> // Include path for Linux
#include <SDL2/SDL_mixer.h>
#endif
#include "MasterClock.h"
#include "AudioPlayer.h"
#include <iostream>
#include <vector>
#include <chrono>

class AudioLooper {
public:
    AudioLooper(MasterClock& mc, AudioPlayer* player, double interval, bool verbose,
        std::string keypadID);
    // Move Constructor
    AudioLooper(AudioLooper&& other) noexcept;
    ~AudioLooper();

    void fetchBPM();
    void startLoop();
    void stopLoop();
    bool checkIfLooping();
    const std::string& getFilePath() const;
    void setSchedule();

    // Move Assignment Operator
    AudioLooper& operator=(AudioLooper&& other) noexcept;

private:
    void setIDTag();

    MasterClock& masterClock;
    AudioPlayer* player;
    double bpm; // Beats per minute
    double loopInterval; // Loop interval in beats
    bool isLooping; // Whether the audio is currently looping
    double beatDivisions; // Number of divisions per beat from the MasterClock.
    bool verbose;
    std::string keyID;
    std::chrono::high_resolution_clock::duration divisionDurationAsDuration;
    std::chrono::high_resolution_clock::duration intervalDuration;
    std::vector<std::chrono::high_resolution_clock::time_point> intervalTimes;
    std::string idTag;
};
#endif // AUDIO_LOOPER_H