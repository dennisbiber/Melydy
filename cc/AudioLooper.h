// AudioLooper.h
#ifndef AUDIO_LOOPER_H
#define AUDIO_LOOPER_H

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
    std::string startLoop();
    void stopLoop();
    bool checkIfLooping();
    const std::string& getFilePath() const;
    std::string getIDTag();

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
    Duration divisionDurationAsDuration;
    Duration intervalDuration;
    std::string idTag;
};
#endif // AUDIO_LOOPER_H