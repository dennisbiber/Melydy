#ifndef MASTER_CLOCK_H
#define MASTER_CLOCK_H

#include "StreamClock.h"
#include "ScheduleAction.h"
#include "BatchActions.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <deque>
#include <vector>
#include <thread>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <functional>

// The scheduler. Time is measured on StreamClock (the GStreamer pipeline
// clock), and every batch lives on the beat grid:
//
//     grid division k  ==  running time k * divisionDuration
//
// A batch due at grid time T is executed early, at T - lookahead, and its
// actions read getTickTime() == T. Audio actions stamp T on their buffers,
// so how late the thread wakes up no longer changes when the sound plays.
class MasterClock {
public:
    explicit MasterClock(double bpm = 120.0, double beatDivisions = 8.0, bool vb = false, bool sVb = false, bool timeVerbose = false);
    ~MasterClock();
    void setBPM(double newBPM);
    void start();
    void stop();
    std::mutex mtx;

    TimePoint getCurrentTime() const;

    double getBPM() const;
    double getBeatDivisions() const;
    Duration fetchDivisionDurationAsDuration() const;
    int getCurrentDivisonOfBeat();

    // The grid time of the batch currently executing. Actions schedule
    // their audio at this time instead of "now".
    TimePoint getTickTime() const;
    // First grid division at or after t.
    TimePoint nextGridPoint(TimePoint t) const;
    // How far ahead of a tick its batch is executed. Must cover the worst
    // case scheduler wake-up delay plus task processing time.
    void setLookahead(Duration lookahead);
    Duration getLookahead() const;

    // Process Timer
    void startTimer(const std::string& processName, bool start);
    std::string getDurationString(const std::string& processName);
    Duration getDuration(const std::string& processName);
    void writeStringToFile(const std::string& content);
    void executeScheduledBatches();
    void setRuntimeTasks(std::function<void()> taskFunction);

    bool containsBatchActions(const std::string& idTag) const;
    void removeBatchFromQueue(const std::string& idTag);
    void addItemToBatchAtInterval(std::function<void()> function, Duration interval,
        const std::string& idTag, bool isLooping);

private:
    // Adds/removes requested while the scheduler is iterating are queued
    // and applied between passes, so the batch list never changes under
    // the loop that is walking it.
    struct PendingChange {
        bool isRemoval;
        std::string idTag;
        std::function<void()> function;
        Duration interval;
        TimePoint executionTime;
        bool isLooping;
    };

    Duration calculateDivisionDuration() const;
    static void setVerboseStatus(bool vb, bool sVb, bool tVb);
    void applyPendingChanges();
    void logWriterLoop();

    // declare member variables
    double bpm;
    std::atomic<bool> quit;
    double beatDivisions;
    static bool verbose;
    static bool superVerbose;
    static bool timeVerbose;
    int currentDivisionOfBeat;
    std::string filename;

    // declare time units
    Duration divisionDurationAsDuration;
    Duration lookahead;
    Duration processingDuration;
    TimePoint tickTime;
    bool inTick;
    std::thread::id schedulerThreadId;

    // declare structures
    std::vector<std::pair<std::string, std::pair<std::chrono::steady_clock::time_point,
        std::chrono::steady_clock::time_point>>> processRecords;
    std::deque<BatchActions> scheduledActionBatches;
    std::vector<PendingChange> pendingChanges;

    // declare threading mechanisms
    mutable std::mutex timerMutex;
    mutable std::recursive_mutex batchesMutex;   // scheduler holds it for a whole pass
    mutable std::mutex pendingMutex;

    // Logging happens on its own thread so file I/O can't delay the scheduler.
    std::thread logThread;
    std::mutex logMutex;
    std::condition_variable logCV;
    std::vector<std::string> logQueue;
    bool logStop;
};

#endif // MASTER_CLOCK_H
