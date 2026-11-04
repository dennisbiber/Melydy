#include "AudioPlayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

AudioPlayer::AudioPlayer(bool verbose, const char* filepath, AudioProcessor& audioProcessor, GstAudioEngine& engine)
    : audioProcessor(audioProcessor), engine(engine), verbose(verbose), isPlaying(false), filepath(filepath) {
    std::vector<float> decoded = engine.decodeFile(this->filepath);
    if (decoded.empty()) {
        printf("         ---AudioPlayer::AudioPlayer:::Error loading audio file: %s\n", filepath);
        throw std::runtime_error("Failed to load audio file");
    }
    audioProcessor.normalizeAudioSamples(decoded.data(), decoded.size(), 0.8f);
    // Every hit holds a voice until its buffer ends, so an inaudible tail
    // (common in mp3 piano/pad samples) wastes voices. Cut where it stays
    // below -60 dB and fade the last 5 ms so the cut doesn't click.
    const size_t originalFrames = decoded.size() / 2;
    size_t endFrame = originalFrames;
    while (endFrame > 0 && std::fabs(decoded[2 * (endFrame - 1)]) < 0.001f &&
           std::fabs(decoded[2 * (endFrame - 1) + 1]) < 0.001f) {
        --endFrame;
    }
    const size_t fadeFrames = std::min<size_t>(endFrame, static_cast<size_t>(engine.getSampleRate()) / 200);
    for (size_t i = 0; i < fadeFrames; ++i) {
        float gain = static_cast<float>(fadeFrames - i) / static_cast<float>(fadeFrames);
        decoded[2 * (endFrame - fadeFrames + i)] *= gain;
        decoded[2 * (endFrame - fadeFrames + i) + 1] *= gain;
    }
    decoded.resize(endFrame * 2);
    samples = std::make_shared<const std::vector<float>>(std::move(decoded));
    if (verbose) {
        printf("         AudioPlayer::AudioPlayer::%s - Loaded.\n", filepath);
        printf("         AudioPlayer::AudioPlayer::Rate %d, %.2f s (trimmed from %.2f s)\n", engine.getSampleRate(),
            samples->size() / 2.0 / engine.getSampleRate(), originalFrames / static_cast<double>(engine.getSampleRate()));
        printf("         AudioPlayer::AudioPlayer::Constructed.\n");
    }
}

AudioPlayer::~AudioPlayer() {
    // Nothing to free by hand: buffers still queued in GStreamer hold their
    // own shared_ptr to the samples.
}

bool AudioPlayer::getIsPlaying() const {
    return isPlaying;
}

const std::string& AudioPlayer::getFilePath() const {
    return filepath;
}

void AudioPlayer::playAt(TimePoint when) {
    if (verbose) {
        printf("         AudioPlayer::playAt::%s at %lld us\n", filepath.c_str(),
            static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(when.time_since_epoch()).count()));
    }
    if (!engine.scheduleAt(samples, when) && verbose) {
        printf("         AudioPlayer::playAt::Not scheduled (no free voice): %s\n", filepath.c_str());
    }
}

void AudioPlayer::stop() {
    // Hits are only queued `lookahead` (~30 ms) ahead, so there is nothing
    // long-lived to cancel. Stopping a looper removes its batch, which stops
    // future hits.
    isPlaying = false;
}
