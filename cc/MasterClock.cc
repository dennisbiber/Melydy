#include "MasterClock.h"
#include <fstream>
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
    isNoteDataReady(false), logging(false), bufferUpdated(false), filename("duration_logs"),
    divisionDurationAsDuration(Duration(0)),
    timeCorrectionForBuffer(Duration(0)), processingDuration(Duration(0)),
    currentDivisionOfBeat(0)
    {
    startTime = getCurrentTime();
    std::time_t startTimeTimeT = std::chrono::high_resolution_clock::to_time_t(startTime);
    std::tm localTime = *std::localtime(&startTimeTimeT);

    // Create a time format string
    std::ostringstream timeFormat;
    timeFormat << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");

    // Convert the ostringstream to a string
    std::string formattedTime = timeFormat.str();
    filename = filename + "_" + formattedTime + ".txt";
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
    divisionDurationAsDuration = calculateDivisionDuration();
}

void MasterClock::stop() {
    quit.store(true, std::memory_order_release);  // Signal the timer thread to quit
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
        printf("   MasterClock::calculateDivisionDuration::Duration: %lld microsecond\n", 
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }
    
    return duration;
}

void MasterClock::writeStringToFile(const std::string& content) {
    std::ofstream outputFile(filename, std::ios_base::app);
    if (outputFile.is_open()) {
        outputFile << content;
        outputFile.close(); // Close the file
    } else {
        std::cerr << "Error opening file for writing!" << std::endl;
    }
}

// Scheduler Section
//###################################################################################################################
bool MasterClock::containsBatchActions(const std::string& idTag) const {
    return searchBatchActions(idTag);
}

void MasterClock::removeBatchFromQueue(const std::string& idTag) {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);

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

bool MasterClock::searchBatchActions(const std::string& idTag) const {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
    return std::any_of(scheduledActionBatches.begin(), scheduledActionBatches.end(),
        [&](const BatchActions& batch) { return batch.getIDTag() == idTag; });
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

void MasterClock::setRuntimeTasks(std::function<void()> taskFunction) {
    std::string idTag = "RunTimeTasks";
    Duration divisionDurationAsDuration = fetchDivisionDurationAsDuration();
    Duration intervalDuration = divisionDurationAsDuration - std::chrono::microseconds(500);

    addItemToBatchAtInterval(taskFunction, intervalDuration, idTag, true);
}

void MasterClock::addItemToBatchAtInterval(std::function<void()> function, Duration interval, 
    const std::string& idTag, bool isLooping) {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
    if (verbose) {
        printf("   MasterClock::addItemToBatchAtInteval::Entered.\n");
        printf("   MasterClock::addItemToBatchAtInterval::idTag: %s.\n", idTag.c_str());
    }

    if (interval.count() > 0) {
        auto it = idTagToBatchIndex.find(idTag);
        if (it != idTagToBatchIndex.end() && scheduledActionBatches[it->second].getDuration() == interval) {
            scheduledActionBatches[it->second].addScheduledAction(ScheduleAction(function));
        } else {
            createNewBatchAndAddAction(function, interval, idTag, isLooping);
        }
    }
}

void MasterClock::createNewBatchAndAddAction(std::function<void()> function, Duration interval, 
    const std::string& idTag, bool isLooping) {
    TimePoint executionTime = getCurrentTime() + interval;
    BatchActions newBatch({ ScheduleAction(function) }, executionTime, idTag, isLooping, interval);
    
    // Find the appropriate position to insert the new batch to maintain sorted order
    auto insertPos = std::lower_bound(scheduledActionBatches.begin(), scheduledActionBatches.end(), newBatch,
        [](const BatchActions& lhs, const BatchActions& rhs) {
            return lhs.getExecutionTime() < rhs.getExecutionTime();
        });

    // Insert the new batch at the calculated position
    scheduledActionBatches.insert(insertPos, std::move(newBatch));
    
    // Update idTagToBatchIndex with the correct index for the newly inserted batch
    idTagToBatchIndex[idTag] = std::distance(scheduledActionBatches.begin(), insertPos);
}

void MasterClock::executeScheduledBatches() {
    while (!quit.load(std::memory_order_acquire)) {
        
        startTimer("ScheduleThreadProcess", true);
        TimePoint currentTime = getCurrentTime();
        TimePoint nextExecutionTime  = TimePoint::max();
        std::string idTagInUse;
        for (auto& batch : scheduledActionBatches) {
            if (batch.getExecutionTime() <= currentTime) {
                batch.executeBatch();
                if (batch.getLoopingFlag()) {
                    if (batch.getIDTag() != "RunTimeTasks") {
                        idTagInUse = batch.getIDTag();
                    }
                    TimePoint nextExecutionTime = currentTime + batch.getDuration();
                    batch.setExecutionTime(nextExecutionTime);
                    idTagToBatchIndex[batch.getIDTag()] = getIndexForBatch(batch);
                    
                }
            }
            nextExecutionTime = std::min(nextExecutionTime, batch.getExecutionTime());
        }
        startTimer("ScheduleThreadProcess", false);
        processingDuration = getDuration("ScheduleThreadProcess");
        TimePoint sleepTimePoint = nextExecutionTime  - processingDuration;
        std::string sleepDurationLog = "MasterClock::executeScheduledBatches::Duration: " +  std::to_string(std::chrono::duration_cast<
            std::chrono::microseconds>(sleepTimePoint - currentTime).count()) + " (microseconds) " + idTagInUse + "\n";
        writeStringToFile(sleepDurationLog);
        if (sleepTimePoint > currentTime) {
            std::this_thread::sleep_until(sleepTimePoint);
        }
        currentDivisionOfBeat += 1;
        if (currentDivisionOfBeat >= beatDivisions) {
            currentDivisionOfBeat = 0;
        }
    }
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

Duration MasterClock::getDuration(const std::string& processName) {
    std::lock_guard<std::mutex> lock(timerMutex);
    for (auto it = processRecords.begin(); it != processRecords.end(); ++it) {
        const auto& record = *it;

        if (record.first == processName && record.second.first != TimePoint() && record.second.second != TimePoint()) {
            const Duration duration = record.second.second - record.second.first;
            // Erase the entry from the vector before returning
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

        if (record.first == processName && record.second.first != TimePoint() && record.second.second != TimePoint()) {
            const std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(record.second.second - record.second.first);
            // Erase the entry from the vector before returning
            processRecords.erase(it);

            return processName + " Duration: " + std::to_string(duration.count()) + " milliseconds\n";
        }
    }
    return "-1"; // If processName not found or start/end time not set
}
