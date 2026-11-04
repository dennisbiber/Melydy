// timing_test.cc
//
// Headless timing check for the GStreamer audio path. It uses the real
// MasterClock, AudioLooper, AudioPlayer and GstAudioEngine, but replaces
// the keyboard with a script and the speakers with a WAV file:
//
//   engine sink = "wavenc ! filesink location=<out.wav>"
//
// Every tick also burns a random 0-20 ms of CPU to simulate a busy
// scheduler. check_timing.py then finds every click in the WAV and checks
// that it sits on the exact sample its grid time maps to.
//
// Usage: timing_test <click.wav> <out.wav> <schedule.csv> [seconds]
#include "../MasterClock.h"
#include "../AudioLooper.h"
#include "../AudioPlayer.h"
#include "../AudioProcessor.h"
#include "../GstAudioEngine.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("usage: %s <click.wav> <out.wav> <schedule.csv> [seconds]\n", argv[0]);
        return 2;
    }
    const std::string clickPath = argv[1];
    const std::string outPath = argv[2];
    const std::string csvPath = argv[3];
    const int seconds = argc > 4 ? std::atoi(argv[4]) : 30;

    // Same musical setup as the README example config.
    const double bpm = 130.0;
    const double beatDivisions = 4.0;
    MasterClock masterClock(bpm, beatDivisions);
    masterClock.start();
    masterClock.setLookahead(std::chrono::milliseconds(50));

    GstAudioEngine::Config config;
    config.sampleRate = 48000;
    config.voices = 32;
    config.sink = "wavenc ! filesink location=\"" + outPath + "\"";
    GstAudioEngine engine(config);
    if (!engine.start()) {
        return 1;
    }

    AudioProcessor processor(false);
    AudioPlayer player(false, clickPath.c_str(), processor, engine);

    // Loop lengths in beats, KP1..KP9 from the README config.
    const double loopBeats[] = {16.0, 8.0, 6.0, 4.0, 3.0, 2.5, 2.0, 1.5, 1.0};
    std::vector<std::shared_ptr<AudioLooper>> loopers;
    std::ofstream csv(csvPath);
    csv << "kind,start_ns,interval_ns\n";

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> busyMicros(0, 20000);
    long tickCount = 0;

    // This plays the role of AudioManager::audioPlaybackTask: on some ticks
    // a "key" is pressed, played on the tick, and recorded into a looper.
    masterClock.setRuntimeTasks([&]() {
        ++tickCount;
        TimePoint tick = masterClock.getTickTime();
        // Press keypad N on tick 20 + 7N: spreads the loops over different
        // beats and divisions so their phases differ.
        for (size_t i = 0; i < 9; ++i) {
            if (tickCount == static_cast<long>(20 + 7 * i)) {
                player.playAt(tick);  // the live hit
                auto looper = std::make_shared<AudioLooper>(masterClock, &player, 1.0 / loopBeats[i], false,
                    "KP" + std::to_string(i + 1));
                looper->startLoop();
                loopers.push_back(looper);
                // AudioLooper's interval is computed exactly like this:
                Duration interval = std::chrono::duration_cast<Duration>(
                    masterClock.fetchDivisionDurationAsDuration() * beatDivisions / (1.0 / loopBeats[i]));
                csv << "loop," << tick.time_since_epoch().count() << "," << interval.count() << "\n";
            }
        }
        // Simulated CPU load: busy-wait, so the scheduler thread really is late.
        auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(busyMicros(rng));
        while (std::chrono::steady_clock::now() < until) {
        }
    });

    std::thread scheduler([&]() { masterClock.executeScheduledBatches(); });
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    masterClock.stop();
    scheduler.join();
    TimePoint endTime = StreamClock::now();
    csv << "end," << endTime.time_since_epoch().count() << ",0\n";
    csv << "division," << masterClock.fetchDivisionDurationAsDuration().count() << ",0\n";
    csv.close();

    for (auto& looper : loopers) {
        looper->stopLoop();
    }
    engine.shutdown();
    printf("timing_test: %ld ticks in %d s, %zu loopers\n", tickCount, seconds, loopers.size());
    return 0;
}
