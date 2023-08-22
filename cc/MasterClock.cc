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
    timeCorrectionForBuffer(Duration(0)), processingDuration(Duration(0))
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
    const std::string& idTag, bool isLooping) {
    std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
    bool keypadFlag = false;

    // Check if a batch with the same idTag and interval exists
    std::string pattern = "KP[1-9]";  // Pattern for idTag
    if (std::regex_match(idTag, std::regex(pattern))) {
        keypadFlag = true;
    }

    if (interval.count() > 0) {
        auto it = idTagToBatchIndex.find(idTag);
        if (it != idTagToBatchIndex.end()) {
            if (scheduledActionBatches[it->second].getDuration() == interval) {
                scheduledActionBatches[it->second].addScheduledAction(ScheduleAction(function));
                // You might want to update the execution time or loop interval here
            } else {
                // Duration mismatch, create a new BatchActions
                createNewBatchAndAddAction(function, interval, idTag, isLooping);
            }
        } else {
            // Batch doesn't exist, create a new BatchActions
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
        TimePoint nextExecutionTime = TimePoint::max();;
        {
            std::lock_guard<std::mutex> lock(scheduledIntervalsMutex);
            timeCorrectionForBuffer += processingDuration;
            for (auto& batch : scheduledActionBatches) {
                if (batch.getExecutionTime() <= currentTime) {
                    if (batch.getIDTag() == "timePointQueue") {
                        batch.executeBatchWithCorrection(timeCorrectionForBuffer);
                        timeCorrectionForBuffer = Duration(0);
                    } else {
                        batch.executeBatch();
                    }
                    if (batch.getLoopingFlag()) {
                        TimePoint nextExecutionTime = currentTime + batch.getDuration();
                        batch.setTimePoint(nextExecutionTime);
                        idTagToBatchIndex[batch.getIDTag()] = getIndexForBatch(batch);
                    }
                }
                nextExecutionTime = std::min(nextExecutionTime, batch.getExecutionTime());
            }
        }
        startTimer("ScheduleThreadProcess", false);
        processingDuration = getDuration("ScheduleThreadProcess");
        Duration sleepDuration = std::chrono::duration_cast<Duration>(nextExecutionTime - currentTime - processingDuration);
        std::string sleepDurationLog = "MasterClock::executeScheduledBatches::Duration: " +  std::to_string(std::chrono::duration_cast<
            std::chrono::microseconds>(sleepDuration).count()) + " (ms)\n";
        writeStringToFile(sleepDurationLog);
        if (sleepDuration > Duration::zero()) {
            std::this_thread::sleep_for(sleepDuration);
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
        this->timePointQueue(this->timeCorrectionForBuffer); // Call the function you want to execute
    }, divisionDurationAsDuration - std::chrono::microseconds(500), "timePointQueue", true);
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
void MasterClock::timePointQueue(Duration correctionTime) {
    std::lock_guard<std::mutex> lock(divisionTimesMutex);
    bufferUpdated = true;
    bufferUpdateCV.notify_all();
    TimePoint lastTimePoint = divisionTimes.back();
    nextDivisionTime = lastTimePoint + divisionDurationAsDuration - correctionTime;
    // Add the next beat time to the vector in a FIFO manner
    if (divisionTimes.size() >= 5) {
        divisionTimes.erase(divisionTimes.begin()); // Remove the oldest beat if the vector is already full
    }
    divisionTimes.push_back(nextDivisionTime);
    std::string timeCorrectionLog = "MasterClock::timePointQueue::TimeCorrection: "
        +  std::to_string(std::chrono::duration_cast<
        std::chrono::microseconds>(correctionTime).count()) + " (microseconds)\n";
    std::string nextEntryLog = "MasterClock::timePointQueue::NextDivisionTime: " +  
        std::to_string(std::chrono::duration_cast<
        std::chrono::microseconds>(nextDivisionTime - getCurrentTime()).count()) + " (microseconds)\n";
    std::string durationCheck = "MasterClock::timePointQueue::CurrentDivisionTime: " +  
        std::to_string(std::chrono::duration_cast<
        std::chrono::microseconds>(divisionTimes[3] - getCurrentTime()).count()) + " (microseconds)\n";
    std::string logEntry = timeCorrectionLog + durationCheck + nextEntryLog;
    writeStringToFile(logEntry);
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
