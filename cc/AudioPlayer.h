#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "StreamClock.h"
#include "AudioProcessor.h"
#include "GstAudioEngine.h"
#include <string>

typedef float Sample;

// One sample file, decoded once at load time and kept in memory.
// Was: a Mix_Chunk played with Mix_PlayChannel ("as soon as possible").
// Now: decoded samples handed to the GStreamer engine with an exact start time.
class AudioPlayer {
public:
    AudioPlayer(bool verbose, const char* filepath, AudioProcessor& audioProcessor, GstAudioEngine& engine);
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Start this sound at running time `when` (normally the tick time the
    // MasterClock is executing for). Sample-accurate.
    void playAt(TimePoint when);
    void stop();
    bool getIsPlaying() const;

    // Get the file path of the loaded audio
    const std::string& getFilePath() const;

private:
    AudioProcessor& audioProcessor;
    GstAudioEngine& engine;
    GstAudioEngine::SampleData samples;
    bool verbose;
    bool isPlaying;
    std::string filepath;
};

#endif // AUDIO_PLAYER_H
