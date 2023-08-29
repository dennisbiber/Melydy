#include "BatchActions.h"
#include <deque>
#include <functional>
#include <chrono>

BatchActions::BatchActions(std::deque<ScheduleAction> batch, TimePoint executionTime, 
    const std::string& identifier, bool looping, Duration loopInterval)
        : batch(std::move(batch)), executionTime(executionTime), idTag(identifier),
        isLooping(looping), interval(loopInterval) {}

// Copy constructor
BatchActions::BatchActions(const BatchActions& other) {
    std::lock_guard<std::mutex> lock(other.actionsMutex);
    batch = other.batch;
    executionTime = other.executionTime;
    idTag = other.idTag;
    isLooping = other.isLooping;
    interval = other.interval;
}

// Move constructor
BatchActions::BatchActions(BatchActions&& other) {
    std::lock_guard<std::mutex> lock(other.actionsMutex);
    batch = std::move(other.batch);
    executionTime = other.executionTime;
    idTag = std::move(other.idTag);
    isLooping = other.isLooping;
    interval = other.interval;
}

BatchActions::~BatchActions() {}

bool BatchActions::operator<(const BatchActions& other) const {
    return executionTime > other.executionTime;
}

void BatchActions::addScheduledAction(ScheduleAction action) {
    std::lock_guard<std::mutex> lock(actionsMutex);
    batch.push_back(std::move(action));
}

void BatchActions::executeBatchWithCorrection(Duration timeCorrection) {
    std::lock_guard<std::mutex> lock(actionsMutex);
    for (auto& action : batch) {
        action.setTimeCorrection(timeCorrection);
        action.execute();
        action.setTimeCorrectionFlagFalse();
    }
}

void BatchActions::executeBatch() const {
    std::lock_guard<std::mutex> lock(actionsMutex);
    for (auto& action : batch) {
        action.execute();
    }
}

void BatchActions::setExecutionTime(TimePoint newExecutionTime) {
    executionTime = newExecutionTime;
}

TimePoint BatchActions::getExecutionTime() const {
    return executionTime;
}

std::string BatchActions::getIDTag() const {
    return idTag;
}

void BatchActions::setDuration(Duration newDuration) {
    interval = newDuration;
}

Duration BatchActions::getDuration() const {
    return interval;
}

bool BatchActions::getLoopingFlag() const {
    return isLooping;
}