// AudioLooper.cc
#include "MasterClock.h"
#include "AudioLooper.h"
#include "AudioPlayer.h"
#include <iostream>
#include <vector>
#include <chrono>

AudioLooper::AudioLooper(MasterClock& mc, AudioPlayer* player, double interval, bool verbose, 
    std::string keypadID) : bpm(mc.getBPM()), 
    loopInterval(interval), isLooping(false), masterClock(mc), 
    player(player), divisionDurationAsDuration(mc.fetchDivisionDurationAsDuration()),
    verbose(verbose), beatDivisions(mc.getBeatDivisions()),
    intervalDuration(std::chrono::duration_cast<
        std::chrono::high_resolution_clock::duration>(divisionDurationAsDuration * beatDivisions / loopInterval)),
    keyID(keypadID) {
    fetchBPM();
    setIDTag();
    if (verbose) {
        auto millisecondsIntervalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(intervalDuration).count();
        printf("         AudioLooper:AudioLooper::Interval Duration = %lld ms.\n", millisecondsIntervalDuration);
        printf("         AudioLooper::AudioLooper::Constructed.\n");
    }
}

// Move Constructor
AudioLooper::AudioLooper(AudioLooper&& other) noexcept
    : bpm(other.bpm),
      loopInterval(other.loopInterval),
      isLooping(other.isLooping),
      masterClock(other.masterClock),
      player(other.player),
      divisionDurationAsDuration(other.divisionDurationAsDuration),
      verbose(other.verbose),
      beatDivisions(other.beatDivisions),
      intervalDuration(other.intervalDuration),
      keyID(other.keyID) {
    fetchBPM();
}


AudioLooper::~AudioLooper() {
    if (isLooping) {
        stopLoop();
    }
    if (verbose) {
        printf("         AudioLooper::~AudioLooper::Entered.\n");
    }
}

void AudioLooper::fetchBPM() {
    bpm = masterClock.getBPM();
    if (verbose) {
        printf("         AudioLooper::fetchBPM::Entered.\n");
    }
}

void AudioLooper::setIDTag() {
    idTag = keyID + "_" + std::to_string(masterClock.getCurrentDivisonOfBeat());
}

std::string AudioLooper::getIDTag() {
    return idTag;
}

void AudioLooper::stopLoop() {
    if (verbose) {
        printf("         AudioLooper::stopLoop::Entered.\n");
    }
    isLooping = false;
    if (masterClock.containsBatchActions(idTag)) {
        masterClock.removeBatchFromQueue(idTag);
        player->stop();
    }
    if (verbose) {
        printf("         AudioLooper::stopLoop::Finished.\n");
    }
}

const std::string& AudioLooper::getFilePath() const {
    return player->getFilePath();
}

bool AudioLooper::checkIfLooping() {
    return isLooping;
}

std::string AudioLooper::startLoop() {
    if (verbose) {
        printf("         AudioLooper::startLoop::Entered.\n");
    }
    isLooping = true;
    masterClock.addItemToBatchAtInterval([&]() {
        this->player->playAudio(); // Call the function you want to execute
    }, intervalDuration, idTag, true);
    return idTag;
}
// printf("FILEPATH: %s\n", player->getFilePath().c_str());
// Assuming you have a variable called 'timeToWait' of type std::chrono::high_resolution_clock::duration
// std::chrono::milliseconds timeToWait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeToWait);
// printf("Time to wait in milliseconds: %lld\n", timeToWait_ms.count());