// GstAudioEngine.h
//
// GStreamer replacement for the SDL_mixer calls Melydy used to make:
//
//   Mix_OpenAudio      -> GstAudioEngine::start()      (build + play the pipeline)
//   Mix_LoadWAV        -> GstAudioEngine::decodeFile() (decodebin -> appsink)
//   Mix_PlayChannel    -> GstAudioEngine::scheduleAt() (timestamped buffer -> appsrc)
//   mixer channels     -> "voices" (one appsrc per voice, all feeding audiomixer)
//
// The pipeline it builds (N = audioMixer.mixer_channels):
//
//   appsrc v0 ─┐
//   appsrc v1 ─┤
//      ...     ├─> audiomixer ─> audioconvert ─> audioresample ─> <sink>
//   appsrc vN ─┘
//
// The key difference from SDL_mixer: Mix_PlayChannel meant "start this as soon
// as you get around to it" (i.e. at the next ~43 ms mix buffer). scheduleAt()
// means "this sound starts at running time T". The buffer carries T as its
// PTS, and audiomixer places it at the exact sample for T, whatever the
// buffer size is.
#ifndef GST_AUDIO_ENGINE_H
#define GST_AUDIO_ENGINE_H

#include "StreamClock.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

typedef struct _GstElement GstElement;
typedef struct _GstAppSrc GstAppSrc;

class GstAudioEngine {
public:
    struct Config {
        int sampleRate = 48000;
        int voices = 8;                          // like SDL_mixer's channel count
        std::string sink = "autoaudiosink";      // any gst-launch sink description
        int mixerLatencyMs = 10;                 // how long audiomixer waits for late data
        bool verbose = false;
    };

    // Decoded audio: interleaved stereo F32 at the engine's sample rate.
    // shared_ptr so a buffer in flight keeps the samples alive (zero-copy).
    using SampleData = std::shared_ptr<const std::vector<float>>;

    explicit GstAudioEngine(const Config& config);
    ~GstAudioEngine();

    bool start();
    void shutdown();   // sends EOS and waits for it (finalizes file sinks)

    // Decode any file GStreamer can read (wav, flac, mp3, ...) into memory.
    std::vector<float> decodeFile(const std::string& path) const;

    // Schedule `samples` to start at running time `when`.
    // Returns false if no voice is free at that time (SDL_mixer's
    // "no free channels" case).
    bool scheduleAt(const SampleData& samples, TimePoint when);

    int getSampleRate() const { return config.sampleRate; }
    Duration getPipelineLatency() const { return pipelineLatency; }

private:
    struct Voice {
        GstAppSrc* src = nullptr;
        TimePoint busyUntil = TimePoint(Duration(0));
    };

    Config config;
    GstElement* pipeline = nullptr;
    std::vector<Voice> voices;
    std::mutex voicesMutex;
    Duration pipelineLatency = Duration(0);
    bool running = false;
    uint64_t lateHits = 0;
    uint64_t droppedHits = 0;
};

#endif // GST_AUDIO_ENGINE_H
