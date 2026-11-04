#include "MasterClock.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <functional>

// Define and initialize the static members
bool MasterClock::verbose = false;
bool MasterClock::superVerbose = false;
bool MasterClock::timeVerbose = false;

namespace {
long long toMicros(Duration d) {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}
} // namespace

MasterClock::MasterClock(double bpm, double beatDivisions, bool vb, bool sVb, bool timeVerbose) : quit(false),
    currentDivisionOfBeat(0), filename("duration_logs"),
    divisionDurationAsDuration(Duration(0)), lookahead(std::chrono::milliseconds(50)),
    processingDuration(Duration(0)), tickTime(TimePoint(Duration(0))), inTick(false),
    logStop(false)
    {
    // Wall-clock time is only used to name the log file.
    std::time_t startTimeTimeT = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm localTime = *std::localtime(&startTimeTimeT);
    std::ostringstream timeFormat;
    timeFormat << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
    filename = filename + "_" + timeFormat.str() + ".txt";

    logThread = std::thread(&MasterClock::logWriterLoop, this);
    setVerboseStatus(vb, sVb, timeVerbose);
    this->setBPM(bpm);
    this->beatDivisions = beatDivisions;
    std::string logMessage = "MasterClock::MasterClock::Setup:\n";
    logMessage += "  BPM: " + std::to_string(bpm) + "\n";
    logMessage += "  Divisions: " + std::to_string(beatDivisions) + "\n";
    writeStringToFile(logMessage);
    if (verbose) {
        printf("   MasterClock::masterClock::Constructred\n");
    }
}

MasterClock::~MasterClock(){
    stop();
    {
        std::lock_guard<std::mutex> lock(logMutex);
        logStop = true;
    }
    logCV.notify_one();
    if (logThread.joinable()) {
        logThread.join();
    }
}
// Start/Stop Section
//###################################################################################################################
void MasterClock::start() {
    divisionDurationAsDuration = calculateDivisionDuration();
}

void MasterClock::stop() {
    quit.store(true, std::memory_order_release);  // Signal the scheduler loop to quit
}
// Getter/Setter Section
//###################################################################################################################
void MasterClock::setVerboseStatus(bool vb, bool sVb, bool tVb) {
    verbose = vb;
    superVerbose = sVb;
    timeVerbose = tVb;
}

void MasterClock::setBPM(double newBPM) {
    if (newBPM > 0) {
        bpm = newBPM;
    }
}

double MasterClock::getBPM() const {
    return bpm;
}

int MasterClock::getCurrentDivisonOfBeat() {
    return currentDivisionOfBeat;
}

TimePoint MasterClock::getCurrentTime() const {
    return StreamClock::now();
}

TimePoint MasterClock::getTickTime() const {
    return tickTime;
}

void MasterClock::setLookahead(Duration newLookahead) {
    if (newLookahead.count() > 0) {
        lookahead = newLookahead;
    }
}

Duration MasterClock::getLookahead() const {
    return lookahead;
}

TimePoint MasterClock::nextGridPoint(TimePoint t) const {
    const Duration::rep div = divisionDurationAsDuration.count();
    if (div <= 0) {
        return t;
    }
    const Duration::rep ns = t.time_since_epoch().count();
    const Duration::rep k = ns <= 0 ? 0 : (ns + div - 1) / div;  // ceil
    return TimePoint(Duration(k * div));
}

Duration MasterClock::fetchDivisionDurationAsDuration() const {
    return divisionDurationAsDuration;
}

double MasterClock::getBeatDivisions() const {
    return beatDivisions;
}

Duration MasterClock::calculateDivisionDuration() const {
    // Calculate the duration of one beat based on the BPM
    Duration duration = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>((60.0 / bpm) / beatDivisions));
    if (verbose) {
        printf("   MasterClock::calculateDivisionDuration::BPM: %f\n", bpm);
        printf("   MasterClock::calculateDivisionDuration::beatDivisions: %f\n", beatDivisions);
        printf("   MasterClock::calculateDivisionDuration::Duration: %lld microsecond\n", toMicros(duration));
    }
    return duration;
}

// Logging Section
//###################################################################################################################
void MasterClock::writeStringToFile(const std::string& content) {
    // Just queue it; logWriterLoop does the file I/O off the scheduler thread.
    {
        std::lock_guard<std::mutex> lock(logMutex);
        logQueue.push_back(content);
    }
    logCV.notify_one();
}

void MasterClock::logWriterLoop() {
    std::ofstream outputFile(filename, std::ios_base::app);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
    }
    std::vector<std::string> batch;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(logMutex);
            logCV.wait(lock, [this] { return logStop || !logQueue.empty(); });
            batch.swap(logQueue);
            if (batch.empty() && logStop) {
                break;
            }
        }
        if (outputFile.is_open()) {
            for (const auto& line : batch) {
                outputFile << line;
            }
            outputFile.flush();
        }
        batch.clear();
    }
}

// Scheduler Section
//###################################################################################################################
bool MasterClock::containsBatchActions(const std::string& idTag) const {
    {
        std::lock_guard<std::recursive_mutex> lock(batchesMutex);
        if (std::any_of(scheduledActionBatches.begin(), scheduledActionBatches.end(),
                [&](const BatchActions& batch) { return batch.getIDTag() == idTag; })) {
            return true;
        }
    }
    std::lock_guard<std::mutex> lock(pendingMutex);
    return std::any_of(pendingChanges.begin(), pendingChanges.end(),
        [&](const PendingChange& change) { return !change.isRemoval && change.idTag == idTag; });
}

void MasterClock::removeBatchFromQueue(const std::string& idTag) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    pendingChanges.push_back({true, idTag, nullptr, Duration(0), TimePoint(), false});
}

void MasterClock::setRuntimeTasks(std::function<void()> taskFunction) {
    addItemToBatchAtInterval(taskFunction, fetchDivisionDurationAsDuration(), "RunTimeTasks", true);
}

void MasterClock::addItemToBatchAtInterval(std::function<void()> function, Duration interval,
    const std::string& idTag, bool isLooping) {
    if (verbose) {
        printf("   MasterClock::addItemToBatchAtInteval::Entered.\n");
        printf("   MasterClock::addItemToBatchAtInterval::idTag: %s.\n", idTag.c_str());
    }
    if (interval.count() <= 0) {
        return;
    }
    // Anchor the first execution to the grid, never to "now":
    //  - called from inside a tick (e.g. a key press starting a looper):
    //    the sound was just played at tickTime, so the repeat is exactly
    //    one interval after it.
    //  - called from outside the scheduler (startup): the next grid point
    //    we can still reach in time.
    const bool calledFromTick = inTick && std::this_thread::get_id() == schedulerThreadId;
    TimePoint first = calledFromTick ? tickTime + interval
                             : nextGridPoint(getCurrentTime() + lookahead);
    std::lock_guard<std::mutex> lock(pendingMutex);
    pendingChanges.push_back({false, idTag, std::move(function), interval, first, isLooping});
}

void MasterClock::applyPendingChanges() {
    std::vector<PendingChange> changes;
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        changes.swap(pendingChanges);
    }
    for (auto& change : changes) {
        if (change.isRemoval) {
            scheduledActionBatches.erase(std::remove_if(scheduledActionBatches.begin(), scheduledActionBatches.end(),
                [&](const BatchActions& batch) { return batch.getIDTag() == change.idTag; }),
                scheduledActionBatches.end());
            continue;
        }
        // Join an existing batch only if it really is the same slot: same
        // tag, same interval, AND same phase. (Matching on tag + interval
        // alone let a loop recorded on a later beat join an earlier loop's
        // batch and play at the earlier loop's phase.)
        auto same = std::find_if(scheduledActionBatches.begin(), scheduledActionBatches.end(),
            [&](const BatchActions& batch) {
                return batch.getIDTag() == change.idTag && batch.getDuration() == change.interval &&
                    batch.getExecutionTime() == change.executionTime;
            });
        if (same != scheduledActionBatches.end()) {
            same->addScheduledAction(ScheduleAction(change.function));
            continue;
        }
        BatchActions newBatch({ ScheduleAction(change.function) }, change.executionTime,
            change.idTag, change.isLooping, change.interval);
        auto insertPos = std::lower_bound(scheduledActionBatches.begin(), scheduledActionBatches.end(), newBatch,
            [](const BatchActions& lhs, const BatchActions& rhs) {
                return lhs.getExecutionTime() < rhs.getExecutionTime();
            });
        scheduledActionBatches.insert(insertPos, std::move(newBatch));
    }
}

void MasterClock::executeScheduledBatches() {
    const Duration::rep div = divisionDurationAsDuration.count();
    schedulerThreadId = std::this_thread::get_id();
    while (!quit.load(std::memory_order_acquire)) {
        std::unique_lock<std::recursive_mutex> lock(batchesMutex);
        applyPendingChanges();

        auto processStart = std::chrono::steady_clock::now();
        TimePoint currentTime = getCurrentTime();
        std::string idTagInUse;
        bool executedOneShot = false;

        for (auto& batch : scheduledActionBatches) {
            // Run anything whose tick falls inside the lookahead window.
            if (batch.getExecutionTime() - lookahead > currentTime) {
                continue;
            }
            tickTime = batch.getExecutionTime();
            inTick = true;
            if (div > 0) {
                currentDivisionOfBeat = static_cast<int>(
                    (tickTime.time_since_epoch().count() / div) % static_cast<Duration::rep>(beatDivisions));
            }
            idTagInUse = batch.getIDTag();
            // Lead = how far ahead of the sound we are right before running
            // the batch (after earlier batches in this pass took their time).
            // Must stay positive or the start of the sound gets clipped.
            writeStringToFile("MasterClock::executeScheduledBatches::Lead: " +
                std::to_string(toMicros(tickTime - getCurrentTime())) + " (microseconds) " + idTagInUse + "\n");

            batch.executeBatch();
            inTick = false;

            if (batch.getLoopingFlag()) {
                // THE drift fix: the next tick is computed from the previous
                // *tick*, not from when the thread happened to wake up. Wake-up
                // delays no longer pile up from one repeat to the next.
                TimePoint next = tickTime + batch.getDuration();
                int skipped = 0;
                while (next - lookahead <= currentTime) {  // stalled past whole ticks: skip, don't burst
                    next += batch.getDuration();
                    ++skipped;
                }
                if (skipped > 0) {
                    writeStringToFile("MasterClock::executeScheduledBatches::Skipped: " +
                        std::to_string(skipped) + " ticks " + idTagInUse + "\n");
                }
                batch.setExecutionTime(next);
            } else {
                batch.setDuration(Duration(-1));  // mark one-shot as finished
                executedOneShot = true;
            }
        }
        if (executedOneShot) {
            scheduledActionBatches.erase(std::remove_if(scheduledActionBatches.begin(), scheduledActionBatches.end(),
                [](const BatchActions& batch) { return !batch.getLoopingFlag() && batch.getDuration().count() < 0; }),
                scheduledActionBatches.end());
        }
        applyPendingChanges();  // pick up anything the actions just added/removed

        TimePoint nextExecutionTime = TimePoint::max();
        for (const auto& batch : scheduledActionBatches) {
            nextExecutionTime = std::min(nextExecutionTime, batch.getExecutionTime());
        }
        processingDuration = std::chrono::steady_clock::now() - processStart;

        TimePoint wakeTime = nextExecutionTime == TimePoint::max()
            ? currentTime + std::chrono::milliseconds(5)
            : nextExecutionTime - lookahead;
        if (!idTagInUse.empty()) {
            writeStringToFile("MasterClock::executeScheduledBatches::Duration: " +
                std::to_string(toMicros(processingDuration)) + " (microseconds) PD_time\n");
            writeStringToFile("MasterClock::executeScheduledBatches::Duration: " +
                std::to_string(toMicros(wakeTime - currentTime)) + " (microseconds) " + idTagInUse + "\n");
        }
        // Sleep on the pipeline clock. Let go of the batch lock first so
        // other threads can query the schedule while we wait.
        lock.unlock();
        StreamClock::sleepUntil(wakeTime);
    }
}
// Process Timer Section
//###################################################################################################################
// Profiling uses steady_clock: it's measuring CPU work, not musical time.
void MasterClock::startTimer(const std::string& processName, bool start) {
    std::lock_guard<std::mutex> lock(timerMutex);
    auto now = std::chrono::steady_clock::now();
    auto it = std::find_if(processRecords.begin(), processRecords.end(),
        [&processName](const auto& record) { return record.first == processName; });
    if (start) {
        if (it != processRecords.end()) {
            it->second.first = now;
        } else {
            processRecords.push_back({processName, {now, std::chrono::steady_clock::time_point()}});
        }
    } else if (it != processRecords.end()) {
        it->second.second = now;
    }
}

Duration MasterClock::getDuration(const std::string& processName) {
    std::lock_guard<std::mutex> lock(timerMutex);
    for (auto it = processRecords.begin(); it != processRecords.end(); ++it) {
        const auto& record = *it;
        if (record.first == processName && record.second.first != std::chrono::steady_clock::time_point() &&
            record.second.second != std::chrono::steady_clock::time_point()) {
            const Duration duration = std::chrono::duration_cast<Duration>(record.second.second - record.second.first);
            processRecords.erase(it);
            return duration;
        }
    }
    return Duration(-1); // If processName not found or start/end time not set
}

std::string MasterClock::getDurationString(const std::string& processName) {
    std::lock_guard<std::mutex> lock(timerMutex);
    for (auto it = processRecords.begin(); it != processRecords.end(); ++it) {
        const auto& record = *it;
        if (record.first == processName && record.second.first != std::chrono::steady_clock::time_point() &&
            record.second.second != std::chrono::steady_clock::time_point()) {
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(record.second.second - record.second.first);
            processRecords.erase(it);
            return processName + " Duration: " + std::to_string(duration.count()) + " milliseconds\n";
        }
    }
    return "-1"; // If processName not found or start/end time not set
}
