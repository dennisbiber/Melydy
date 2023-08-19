#include "AudioPlayer.h"
#include <chrono>
#include <thread>

AudioPlayer::AudioPlayer(bool verbose, const char* filepath, AudioProcessor& audioProcessor)
    : filepath(filepath), chunk(nullptr), audioProcessor(audioProcessor),
    isPlaying(false), verbose(verbose), channels(2), format(2), frequency(48000) {
    this->chunk = Mix_LoadWAV(filepath);
    if (chunk == nullptr) {
        printf("         ---AudioPlayer::AudioPlayer:::Error loading WAV file: %s\n", Mix_GetError());
        throw std::runtime_error("Failed to load WAV file");
    } else {
        Mix_QuerySpec(&frequency, &format, &channels);
        Uint16 audioFormat = format & 0xFF; // Extract the least significant byte (8 bits) for the audio format
        Uint16 endianFlag = format & 0x8000;
        if (verbose) {
            printf("         AudioPlayer::AudioPlayer::Mix_LoadWav Success\n");
            printf("         AudioPlayer::AudioPlayer::%s - Loaded.\n", filepath);
            printf("         AudioPlayer::AudioPlayer::Freq Set %d\n", frequency);
            printf("         AudioPlayer::AudioPlayer::Format: %" PRIu16 "\n", audioFormat);
            printf("         AudioPlayer::AudioPlayer::Format Flag: %" PRIu16 "\n", endianFlag);
            printf("         AudioPlayer::AudioPlayer::Channels: %d\n", channels);
            printf("         AudioPlayer::AudioPlayer::Constructed.\n");
        }
        int sampleCount = chunk->alen / sizeof(Sample);
        Sample* samples = reinterpret_cast<Sample*>(chunk->abuf);
        audioProcessor.normalizeAudioSamples(samples, sampleCount, 0.8);
    }
}

// Move Constructor
AudioPlayer::AudioPlayer(AudioPlayer&& other) noexcept
    : filepath(std::move(other.filepath)), chunk(other.chunk),
      frequency(other.frequency), format(other.format), channels(other.channels),
      isPlaying(other.isPlaying), verbose(other.verbose),
      audioProcessor(other.audioProcessor) {
    // Set the other object's chunk pointer to nullptr to avoid double-free
    other.chunk = nullptr;
}

AudioPlayer::~AudioPlayer() {
    if (chunk != nullptr) {
        Mix_FreeChunk(chunk);
    }
}

bool AudioPlayer::getIsPlaying() const {
    return isPlaying;
}

const std::string& AudioPlayer::getFilePath() const {
    return filepath;
}

void AudioPlayer::playAudio() {
    if (this->chunk != nullptr) {
        if (verbose) {
            printf("         AudioPlayer::playAudio::Calling Mix_PlayChannel.\n");
            printf("         AudioPlayer::Playing Filepath: %s\n", filepath.c_str());
        }

        // Check if the audio is already playing; if not, start the playback using SDL_mixer
        int channel = Mix_PlayChannel(-1, this->chunk, 0);
        if (channel == -1) {
            printf("         AudioPlayer::playAudio::Mix_PlayChannel Error: %s\n", Mix_GetError());
        }
        if (verbose) {
            printf("         AudioPlayer::After Playing Filepath: %s\n", filepath.c_str()); // Debug print
        }
    }
    isPlaying = false;
}

void AudioPlayer::stop() {
    if (isPlaying) {
        Mix_HaltChannel(-1);
        isPlaying = false;
    }
}
