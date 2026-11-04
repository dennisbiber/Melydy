// StreamClock.cc
#include "StreamClock.h"
#include <gst/gst.h>
#include <algorithm>
#include <mutex>
#include <thread>

namespace {
std::mutex clockMutex;
GstClock* pipelineClock = nullptr;   // ref held while attached
GstClockTime pipelineBaseTime = 0;
const auto fallbackEpoch = std::chrono::steady_clock::now();

// Grab a ref under the lock so a concurrent detach() can't free it under us.
GstClock* acquireClock(GstClockTime& base) {
    std::lock_guard<std::mutex> lock(clockMutex);
    if (pipelineClock == nullptr) {
        return nullptr;
    }
    base = pipelineBaseTime;
    return GST_CLOCK(gst_object_ref(pipelineClock));
}
} // namespace

StreamClock::time_point StreamClock::now() {
    GstClockTime base = 0;
    GstClock* clock = acquireClock(base);
    if (clock == nullptr) {
        auto elapsed = std::chrono::steady_clock::now() - fallbackEpoch;
        return time_point(std::chrono::duration_cast<duration>(elapsed));
    }
    // Running time = absolute clock time - the pipeline's base time.
    GstClockTime absolute = gst_clock_get_time(clock);
    gst_object_unref(clock);
    return time_point(duration(static_cast<rep>(absolute) - static_cast<rep>(base)));
}

void StreamClock::sleepUntil(time_point t) {
    GstClockTime base = 0;
    GstClock* clock = acquireClock(base);
    if (clock == nullptr) {
        auto target = fallbackEpoch + t.time_since_epoch();
        std::this_thread::sleep_until(target);
        return;
    }
    // Wait at most 20 ms per call. The clock is usually the audio device's
    // clock now; if the device stalls (Bluetooth drops, sound server
    // freezes) an uncapped wait could block forever and hang shutdown.
    // Callers (the scheduler loop) re-check their state and sleep again.
    const GstClockTime nowRunning = gst_clock_get_time(clock) - base;
    const GstClockTime target = static_cast<GstClockTime>(std::max<StreamClock::rep>(0, t.time_since_epoch().count()));
    if (target > nowRunning) {
        const GstClockTime capped = std::min<GstClockTime>(target, nowRunning + 20 * GST_MSECOND);
        GstClockID id = gst_clock_new_single_shot_id(clock, base + capped);
        gst_clock_id_wait(id, nullptr);
        gst_clock_id_unref(id);
    }
    gst_object_unref(clock);
}

void StreamClock::attach(GstClock* clock, uint64_t baseTime) {
    std::lock_guard<std::mutex> lock(clockMutex);
    if (pipelineClock != nullptr) {
        gst_object_unref(pipelineClock);
    }
    pipelineClock = GST_CLOCK(gst_object_ref(clock));
    pipelineBaseTime = baseTime;
}

void StreamClock::detach() {
    std::lock_guard<std::mutex> lock(clockMutex);
    if (pipelineClock != nullptr) {
        gst_object_unref(pipelineClock);
        pipelineClock = nullptr;
    }
}

bool StreamClock::isAttached() {
    std::lock_guard<std::mutex> lock(clockMutex);
    return pipelineClock != nullptr;
}
