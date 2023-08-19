#include "MasterClock.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <functional>
#include <regex>

// Define and initialize the static members
bool MasterClock::verbose = false;
bool MasterClock::superVerbose = false;
bool MasterClock::timeVerbose = false;

MasterClock::MasterClock(double bpm, double beatDivisions, bool vb, bool sVb, bool timeVerbose) : quit(false), 
    isNoteDataReady(false), logging(false), bufferUpdated(false), 
    divisionDurationAsDuration(std::chrono::high_resolution_clock::duration(0))
    {
    setVerboseStatus(vb, sVb, timeVerbose);
    this->setBPM(bpm);
    this->beatDivisions = beatDivisions;
    if (verbose) {
        printf("   MasterClock::masterClock::Constructred\n");
    }
}

MasterClock::~MasterClock(){
    stop();
}
// Start/Stop Section
//###################################################################################################################
void MasterClock::start() {
    // Initialize the startTime when starting the clock
    startTime = getCurrentTime();

    // Calculate the duration of one beat based on the BPM
    divisionDurationAsDuration = calculateDivisionDuration();
    if (verbose) {
        printf("   MasterClock::start::Init TimePoint Queue.\n");
    }
    // Start the timer thread for synchronized playback
    initTimePointQueue();
    if (verbose) {
        printf("   MasterClock::start::Starting Thread.\n");
    }
    timerThread = std::thread(&MasterClock::timerLoop, this);
}

bool MasterClock::getQuitState() {
    return quit;
}

void MasterClock::stop() {
    quit.store(true, std::memory_order_release);  // Signal the timer thread to quit
    if (timerThread.joinable()) {
        timerThread.join(); // Wait for the timer thread to finish
    }
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

TimePoint MasterClock::getCurrentTime() const {
    return std::chrono::high_resolution_clock::now();
}


Duration MasterClock::fetchDivisionDurationAsDuration() const {
    return divisionDurationAsDuration;
}

double MasterClock::getBeatDivisions() const {
    return beatDivisions;
}

// TimePoint MasterClock::getStartTime() const {
//     return startTime;
// }

Duration MasterClock::calculateDivisionDuration() const {
    // Calculate the duration of one beat based on the BPM
    Duration duration = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>((60.0 / bpm) / beatDivisions));
    if (verbose) {
        printf("   MasterClock::calculateDivisionDuration::BPM: %f\n", bpm);
        printf("   MasterClock::calculateDivisionDuration::beatDivisions: %f\n", beatDivisions);
        printf("   MasterClock::calculateDivisionDuration::Duration: %lld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }
    
    return duration;
}

TimePoint MasterClock::getDivisionTimePoint(int index) const {
    std::lock_guard<std::mutex> lock(divisionTimesMutex);
    // If the index is negative, convert it to a positive index from the end
    if (index < 0) {
        index += static_cast<int>(divisionTimes.size());
    }

    // Check if the final index is still out of bounds
    if (index < 0 || static_cast<std::size_t>(index) >= divisionTimes.size()) {
        if (verbose) {
            printf("   ---MasterClock::getDivisionTimePoint::Index out of bounds.\n");
        }
        return TimePoint::min();
    }
    return divisionTimes[index];
}

void MasterClock::waitForBufferUpdate() {
    std::unique_lock<std::mutex> lock(divisionTimesMutex);
    bufferUpdateCV.wait(lock, [this] { return bufferUpdated; });
    bufferUpdated = false; // Reset the flag
}

// Scheduler Section
//###################################################################################################################
// bool MasterClock::containsScheduledAction(const std::string& idTag) {
//     std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
//     return idTagToIndex.find(idTag) != idTagToIndex.end();
// }

// void MasterClock::removeScheduledAction(const std::string& idTag) {
//     if (verbose && superVerbose) {
//         printf("   MasterClock::removeScheudledAction::ID Tag: %s.\n", idTag.c_str());
//     }
//     std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
//     auto it = idTagToIndex.find(idTag);
//     if (it != idTagToIndex.end()) {
//         size_t indexToRemove = it->second;
//         idTagToIndex.erase(it);
//         if (indexToRemove != scheduledActions.size() - 1) {
//             std::swap(scheduledActions[indexToRemove], scheduledActions.back());
//             idTagToIndex[scheduledActions[indexToRemove].fetchIDTag()] = indexToRemove;
//         }

//         scheduledActions.pop_back();
//     }
// }

// void MasterClock::modifyScheduledInterval(const std::string& idTag, Duration interval) {
//     std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
//     auto it = idTagToIndex.find(idTag);
//     if (it != idTagToIndex.end()) {
//         scheduledActions[it->second].updatedDuration(interval);
//     }
// }

// void MasterClock::scheduleActionAtInterval(std::function<void()> function, Duration interval, const std::string& idTag) {
//     if (verbose && superVerbose) {
//         printf("   MasterClock::scheduleActionAtInterval::ID Tag: %s.\n", idTag.c_str());
//     }
//     std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);

//     // Calculate the execution time for the first occurrence
//     TimePoint executionTime = getCurrentTime() + interval;

//     ScheduleAction action(function, executionTime, true, interval, idTag);

//     // Use lower_bound to find the correct insertion position
//     auto it = std::lower_bound(scheduledActions.begin(), scheduledActions.end(), action,
//         [](const ScheduleAction& lhs, const ScheduleAction& rhs) {
//             return lhs.fetchTimePoint() < rhs.fetchTimePoint();
//         });

//     // Insert the action at the determined position
//     scheduledActions.insert(it, action);
//     idTagToIndex.emplace(idTag, it - scheduledActions.begin());
// }

// void MasterClock::executeScheduledActions() {
//     std::this_thread::sleep_for(std::chrono::microseconds(500));
//     std::vector<ScheduleAction> nextCyclesAction;
//     std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);

//     while (!scheduledActions.empty() && scheduledActions.front().fetchTimePoint() <= getCurrentTime()) {
//         ScheduleAction action = std::move(scheduledActions.front());
//         scheduledActions.pop_front();
//         action.execute();

//         if (action.fetchLoopingFlag()) {
//             TimePoint executionTime = getCurrentTime() + action.fetchDuration();
//             action.updateTimePoint(executionTime);

//             // Reinsert the action at the correct position
//             auto it = std::lower_bound(scheduledActions.begin(), scheduledActions.end(), action,
//                 [](const ScheduleAction& lhs, const ScheduleAction& rhs) {
//                     return lhs.fetchTimePoint() < rhs.fetchTimePoint();
//                 });

//             scheduledActions.insert(it, std::move(action));
//         }
//     }
// }
bool MasterClock::containsBatchActions(const std::string& idTag) const {
    return searchBatchActions(idTag);
}

void MasterClock::removeBatchFromQueue(const std::string& idTag) {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);

    if (containsBatchActions(idTag)) {
        auto it = std::find_if(scheduledActionBatches.begin(), scheduledActionBatches.end(),
                               [&idTag](const BatchActions& batch) { return batch.getIDTag() == idTag; });

        if (it != scheduledActionBatches.end()) {
            size_t indexToRemove = std::distance(scheduledActionBatches.begin(), it);
            idTagToBatchIndex.erase(idTag);

            scheduledActionBatches.erase(it);

            // Update the indices in idTagToBatchIndex for the remaining batches
            for (size_t i = indexToRemove; i < scheduledActionBatches.size(); ++i) {
                idTagToBatchIndex[scheduledActionBatches[i].getIDTag()] = i;
            }
        }
    }
}

bool MasterClock::searchBatchActions(const std::string& idTag) const {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
    for (const BatchActions& batch : scheduledActionBatches) {
        if (batch.getIDTag() == idTag) {
            return true;
        }
    }
    return false;
}

size_t MasterClock::getIndexForBatch(const BatchActions& batch) const {
    auto it = std::find_if(scheduledActionBatches.begin(), scheduledActionBatches.end(),
                           [&batch](const BatchActions& b) { return &b == &batch; });

    if (it != scheduledActionBatches.end()) {
        return std::distance(scheduledActionBatches.begin(), it);
    }

    // Return an invalid index or throw an exception if not found
    return std::numeric_limits<size_t>::max();
}


void MasterClock::addItemToBatchAtInterval(std::function<void()> function, Duration interval, 
    const std::string& idTag) {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);

    // Check if a batch with the same idTag and interval exists
    std::string pattern = "KP[1-9]";  // Pattern for idTag
    if (std::regex_match(idTag, std::regex(pattern)) && interval.count() > 0) {
        auto it = idTagToBatchIndex.find(idTag);
        if (it != idTagToBatchIndex.end()) {
            if (scheduledActionBatches[it->second].getDuration() == interval) {
                scheduledActionBatches[it->second].addScheduledAction(ScheduleAction(function));
                // You might want to update the execution time or loop interval here
            } else {
                // Duration mismatch, create a new BatchActions
                createNewBatchAndAddAction(function, interval, idTag);
            }
        } else {
            // Batch doesn't exist, create a new BatchActions
            createNewBatchAndAddAction(function, interval, idTag);
        }
    }
}

void MasterClock::createNewBatchAndAddAction(std::function<void()> function, Duration interval, 
    const std::string& idTag) {
    TimePoint executionTime = getCurrentTime() + interval;
    BatchActions newBatch({ ScheduleAction(function) }, executionTime, idTag, false, interval);
    scheduledActionBatches.push_back(std::move(newBatch));
    idTagToBatchIndex[idTag] = scheduledActionBatches.size() - 1;
}

void MasterClock::executeScheduledBatches() {
    while (!quit.load(std::memory_order_acquire)) {
        TimePoint currentTime = getCurrentTime();
        TimePoint nextExecutionTime = TimePoint::max();
        // Calculate the sleep duration until the next execution
        if (nextExecutionTime > currentTime) {
            Duration sleepDuration = std::chrono::duration_cast<Duration>(nextExecutionTime - currentTime);
            std::this_thread::sleep_for(sleepDuration);
        }

        {
            std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
            for (auto& batch : scheduledActionBatches) {
                if (batch.getExecutionTime() <= currentTime) {
                    batch.executeBatch();
                    if (batch.getLoopingFlag()) {
                        TimePoint newExecutionTime = currentTime + batch.getDuration();
                        batch.setTimePoint(newExecutionTime);
                        idTagToBatchIndex[batch.getIDTag()] = getIndexForBatch(batch);
                    }
                } else {
                    // Calculate the time until the next batch's execution
                    nextExecutionTime = std::min(nextExecutionTime, batch.getExecutionTime());
                }
            }
        }
    }
}


// Timer Thread Loop Section
//###################################################################################################################
void MasterClock::timerLoop() {
    if (verbose) {
        printf("   MasterClock::timerLoop::Entered.\n");
    }
    addItemToBatchAtInterval([&]() {
        this->timePointQueue(); // Call the function you want to execute
    }, divisionDurationAsDuration - std::chrono::microseconds(500), "timePointQueue");
    if (verbose) {
        printf("   MasterClock::timerLoop::TimePointQueue Scheudled.\n");
    }
    executeScheduledBatches();
}
// Time Queue Section
//###################################################################################################################
void MasterClock::initTimePointQueue() {
    nextDivisionTime = getCurrentTime();
    nextDivisionTime += divisionDurationAsDuration + divisionDurationAsDuration;
    std::lock_guard<std::mutex> lock(divisionTimesMutex);
    divisionTimes.push_back(nextDivisionTime);
}

// Update the static function to accept MasterClock& argument and remove the redundant arguments
void MasterClock::timePointQueue() {
    std::lock_guard<std::mutex> lock(divisionTimesMutex);
    bufferUpdated = true;
    bufferUpdateCV.notify_all();
    TimePoint lastTimePoint = divisionTimes.back();
    nextDivisionTime = lastTimePoint + divisionDurationAsDuration;
    // Add the next beat time to the vector in a FIFO manner
    if (divisionTimes.size() >= 5) {
        divisionTimes.erase(divisionTimes.begin()); // Remove the oldest beat if the vector is already full
    }
    divisionTimes.push_back(nextDivisionTime);
}
// Process Timer Section
//###################################################################################################################
void MasterClock::startTimer(const std::string& processName, bool start) {
    std::lock_guard<std::mutex> lock(timerMutex);
    std::string currentProcessName = processName;
    if (start) {
        // If the process name already exists in the vector, update its start time
        auto it = std::find_if(processRecords.begin(), processRecords.end(),
            [&currentProcessName](const auto& record) {
                return record.first == currentProcessName;
            });
        if (it != processRecords.end()) {
            it->second.first = getCurrentTime();
        } else {
            // If the process name does not exist, add a new record
            processRecords.push_back({currentProcessName, {getCurrentTime(), TimePoint()}});
        }
    } else {
        // Update the end time for the corresponding process name
        auto it = std::find_if(processRecords.begin(), processRecords.end(),
            [&currentProcessName](const auto& record) {
                return record.first == currentProcessName;
            });
        if (it != processRecords.end()) {
            it->second.second = getCurrentTime();
        }
    }
}

std::string MasterClock::getDurationString(const std::string& processName) {
    std::lock_guard<std::mutex> lock(timerMutex);
    for (auto it = processRecords.begin(); it != processRecords.end(); ++it) {
        const auto& record = *it;

        if (record.first == processName && record.second.first != TimePoint() && record.second.second != TimePoint()) {
            const std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(record.second.second - record.second.first);
            // Erase the entry from the vector before returning
            processRecords.erase(it);

            return processName + " Duration: " + std::to_string(duration.count()) + " milliseconds\n";
        }
    }
    return "-1"; // If processName not found or start/end time not set
}
