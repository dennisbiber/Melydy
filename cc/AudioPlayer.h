#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h> // Include path for Linux
#include <SDL2/SDL_mixer.h>
#endif
#include "AudioProcessor.h"
#include <string>
#include "mutex"
#include "queue"

typedef float Sample;
using Duration = std::chrono::high_resolution_clock::duration;

class AudioPlayer {
public:
    // Constructor
    AudioPlayer(bool verbose, const char* filepath, AudioProcessor& audioProcessor);

    // Move Constructor
    AudioPlayer(AudioPlayer&& other) noexcept;

    // Destructor
    ~AudioPlayer();

    // Play the audio synchronized with the MasterClock beats
    void playAudio();
    void stop();
    bool getIsPlaying() const;

    // Get the file path of the loaded audio
    const std::string& getFilePath() const;

    // Move Assignment Operator
    AudioPlayer& operator=(AudioPlayer&& other) noexcept;

private:
    AudioProcessor& audioProcessor;
    // Variables
    bool verbose;
    bool isPlaying;
    int frequency;
    int channels;
    Mix_Chunk* chunk;
    Uint16 format;
    std::string filepath;
};

#endif // AUDIO_PLAYER_H
