// StreamClock.h
//
// A std::chrono-compatible clock whose "now" is the GStreamer pipeline's
// running time. Every TimePoint in Melydy is measured on this clock, so the
// scheduler, the loopers, and the audio buffers all share ONE time base.
//
// Why: the old code scheduled on the CPU's high_resolution_clock while the
// sound card played at its own crystal's rate. Two clocks drift apart. When the
// pipeline's clock is provided by the audio sink (alsasink/pulsesink do this),
// "now" here literally means "how much audio the sound card has played".
//
// Running time starts at 0 when the pipeline goes to PLAYING, so beat-grid
// division k sits at k * divisionDuration on this clock.
//
// Before a pipeline is attached it falls back to std::chrono::steady_clock
// (relative to first use), so code that doesn't touch audio still runs.
#ifndef STREAM_CLOCK_H
#define STREAM_CLOCK_H

#include <chrono>
#include <cstdint>

typedef struct _GstClock GstClock;

struct StreamClock {
    using rep = int64_t;
    using period = std::nano;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<StreamClock>;
    static constexpr bool is_steady = true;

    static time_point now();

    // Block until the clock reaches t, or at most 20 ms (call again if
    // now() < t). Uses gst_clock_id_wait() on the pipeline clock when
    // attached, so we sleep against the audio device's time, not the CPU's.
    static void sleepUntil(time_point t);

    // Called by GstAudioEngine once the pipeline is PLAYING.
    static void attach(GstClock* clock, uint64_t baseTime);
    static void detach();
    static bool isAttached();
};

using Duration = StreamClock::duration;
using TimePoint = StreamClock::time_point;

#endif // STREAM_CLOCK_H
