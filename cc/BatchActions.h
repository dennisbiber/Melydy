#ifndef BATCH_ACTIONS_H
#define BATCH_ACTIONS_H

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include "ScheduleAction.h"

using Duration = std::chrono::high_resolution_clock::duration;
using TimePoint = std::chrono::high_resolution_clock::time_point;

class BatchActions {
    public:
        explicit BatchActions(std::deque<ScheduleAction> batch, TimePoint executionTime,
            const std::string& identifier, bool looping, Duration loopInterval);
        
        // Copy constructor
        BatchActions(const BatchActions& other);

        // Move constructor
        BatchActions(BatchActions&& other);

        ~BatchActions();
        
        // Copy assignment operator
        BatchActions& operator=(const BatchActions& other) {
            if (this != &other) {
                std::lock(actionsMutex, other.actionsMutex);
                std::lock_guard<std::mutex> self_lock(actionsMutex, std::adopt_lock);
                std::lock_guard<std::mutex> other_lock(other.actionsMutex, std::adopt_lock);
                
                batch = other.batch;
                executionTime = other.executionTime;
                idTag = other.idTag;
                isLooping = other.isLooping;
                interval = other.interval;
            }
            return *this;
        }

        // Move assignment operator
        BatchActions& operator=(BatchActions&& other) {
            if (this != &other) {
                std::lock(actionsMutex, other.actionsMutex);
                std::lock_guard<std::mutex> self_lock(actionsMutex, std::adopt_lock);
                std::lock_guard<std::mutex> other_lock(other.actionsMutex, std::adopt_lock);
                
                batch = std::move(other.batch);
                executionTime = other.executionTime;
                idTag = std::move(other.idTag);
                isLooping = other.isLooping;
                interval = other.interval;
            }
            return *this;
        }

        bool operator<(const BatchActions& other) const;

        void executeBatch() const;
        void executeBatchWithCorrection(Duration timeCorrection);

        void setTimePoint(TimePoint newExecutionTime);
        TimePoint getExecutionTime() const;
        std::string getIDTag() const;
        void setDuration(Duration newDuration);
        Duration getDuration() const;
        bool getLoopingFlag() const;
        void addScheduledAction(ScheduleAction action);

    private:
        std::deque<ScheduleAction> batch;
        TimePoint executionTime;
        std::string idTag;
        bool isLooping;
        Duration interval;
        mutable std::mutex actionsMutex;
};

class BatchActionsomparator {
    public:
        bool operator()(const BatchActions& lhs, const BatchActions& rhs) const {
            return lhs.getExecutionTime() > rhs.getExecutionTime();
        }
};

#endif // BATCH_ACTIONS_H