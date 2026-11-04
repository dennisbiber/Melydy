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
//      ...     ├─> audiomixer ─> [F32 stereo @ rate] ─> audioconvert ─> audioresample ─> <sink>
//   appsrc vN ─┘
//
// The key difference from SDL_mixer: Mix_PlayChannel meant "start this as soon
// as you get around to it" (i.e. at the next ~43 ms mix buffer). scheduleAt()
// means "this sound starts at running time T": the note is written into the
// voice's stream starting at the exact frame for T.
//
// Each voice is a continuous stream pushed in 10 ms chunks by a feeder thread
// just ahead of the clock (silence when idle). Two reasons:
//  - audiomixer always has data on every input, so it never relies on its
//    "some inputs are idle, wait for a timeout" path, which came out silent on
//    PipeWire + Bluetooth.
//  - nothing is committed far ahead, so a note can still be cut off (voice
//    stealing) when every voice is busy.
#ifndef GST_AUDIO_ENGINE_H
#define GST_AUDIO_ENGINE_H

#include "StreamClock.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

typedef struct _GstElement GstElement;
typedef struct _GstAppSrc GstAppSrc;

class GstAudioEngine {
public:
    struct Config {
        int sampleRate = 48000;
        int voices = 32;                         // like SDL_mixer's channel count
        std::string sink = "autoaudiosink";      // any gst-launch sink description
        int mixerLatencyMs = 10;                 // how long audiomixer waits for late data
        bool useSinkClock = true;                // audio device's clock drives everything
        int feedAheadMs = 10;                    // how far ahead of the clock voices are filled
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

    // Schedule `samples` to start at running time `when` (sample-accurate).
    // If every voice is busy, the voice playing the oldest note is "stolen":
    // that note fades out over 5 ms and the new one takes its place.
    bool scheduleAt(const SampleData& samples, TimePoint when);

    // Peak level of the mixed output, updated from GStreamer's streaming
    // thread, read by the UI. Decays toward 0 during silence.
    struct OutputMeter {
        std::atomic<float> level{0.0f};
        std::atomic<float> maxPeak{0.0f};
        std::atomic<unsigned long long> audibleBuffers{0};
        void record(float peak) {
            if (peak > 0.0001f) {
                ++audibleBuffers;
            }
            if (peak > maxPeak.load()) maxPeak.store(peak);
            if (peak > level.load()) level.store(peak); else decay();
        }
        void decay() { level.store(level.load() * 0.9f); }
    };
    float getOutputLevel() const { return outputMeter.level.load(); }
    int getActiveVoices();
    int getVoiceCount() const { return config.voices; }

    int getSampleRate() const { return config.sampleRate; }
    Duration getPipelineLatency() const { return pipelineLatency; }

private:
    // A note playing (or about to play) on a voice. Positions are in frames
    // of engine-rate audio counted from running time 0, so a start time
    // maps to an exact sample.
    struct Note {
        SampleData samples;
        int64_t startFrame = 0;
        int64_t cutFrame = INT64_MAX;   // set when the voice is stolen: fade out, end here
        int64_t endFrame() const;
    };
    // One appsrc feeding audiomixer. It always carries a continuous stream:
    // note audio when playing, silence when idle. The mixer then always has
    // data on every input and never has to fall back on its timeout path
    // (the path that came out silent on PipeWire + Bluetooth).
    struct Voice {
        GstAppSrc* src = nullptr;
        int64_t pushedFrame = 0;        // everything before this is already handed to GStreamer
        std::vector<Note> notes;        // usually 0 or 1; 2 briefly while a stolen note fades
    };

    void feederLoop();
    void pushChunk(Voice& voice, int64_t chunkFrames);
    int64_t frameAt(TimePoint t) const;

    Config config;
    GstElement* pipeline = nullptr;
    std::vector<Voice> voices;
    std::mutex voicesMutex;
    Duration pipelineLatency = Duration(0);
    bool running = false;
    uint64_t lateHits = 0;
    uint64_t stolenVoices = 0;
    std::thread feederThread;
    std::atomic<bool> feederRunning{false};
    OutputMeter outputMeter;
};

#endif // GST_AUDIO_ENGINE_H
