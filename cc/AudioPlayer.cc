#include "AudioPlayer.h"
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
    samples = std::make_shared<const std::vector<float>>(std::move(decoded));
    if (verbose) {
        printf("         AudioPlayer::AudioPlayer::%s - Loaded.\n", filepath);
        printf("         AudioPlayer::AudioPlayer::Rate %d, frames %zu\n", engine.getSampleRate(), samples->size() / 2);
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
