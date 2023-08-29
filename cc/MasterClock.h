#ifndef MASTER_CLOCK_H
#define MASTER_CLOCK_H

#ifdef _WIN32
#include <SDL.h> // Include path for Windows
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h> // Include path for Linux
#include <SDL2/SDL_mixer.h>
#endif
#include "ScheduleAction.h"
#include "BatchActions.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class MasterClock {
public:
    explicit MasterClock(double bpm = 120.0, double beatDivisions = 8.0, bool vb = false, bool sVb = false, bool timeVerbose = false);
    ~MasterClock();
    void setBPM(double newBPM);
    void start();
    void stop();
    double fetchDivisionDurationInSeconds() const;
    std::mutex mtx;
    mutable std::mutex divisionTimesMutex;

    TimePoint getDivisionTimePoint(int index) const;

    TimePoint getCurrentTime() const;
    // TimePoint getStartTime() const;

    double getBPM() const;
    double getBeatDivisions() const;
    Duration fetchDivisionDurationAsDuration() const;
    int getCurrentDivisonOfBeat();

    // Process Timer
    void startTimer(const std::string& processName, bool start);
    std::string getDurationString(const std::string& processName);
    Duration getDuration(const std::string& processName);
    void writeStringToFile(const std::string& content);
    bool isNoteDataEmpty();
    void updateWindwoTimeValues();
    void waitForBufferUpdate();
    void executeScheduledBatches();
    void setRuntimeTasks(std::function<void()> taskFunction);

    bool containsBatchActions(const std::string& idTag) const;
    void removeBatchFromQueue(const std::string& idTag);
    void addItemToBatchAtInterval(std::function<void()> function, Duration interval, 
        const std::string& idTag, bool isLooping);

private:
    // declare member functions
    static void emptyFunction(MasterClock&) {};
    Duration calculateDivisionDuration() const;
    void timePointQueue(Duration correctionTime);
    void initTimePointQueue();
    static void setVerboseStatus(bool vb, bool sVb, bool tVb);
    void createNewBatchAndAddAction(std::function<void()> function, Duration interval,
        const std::string& idTag, bool isLooping);
    bool searchBatchActions(const std::string& idTag) const;
    size_t getIndexForBatch(const BatchActions& batch) const;

    // declare member variables
    double bpm;
    std::atomic<bool> quit;
    double beatDivisions;
    static bool verbose;
    static bool superVerbose;
    static bool timeVerbose;
    bool bufferUpdated;
    bool isNoteDataReady;
    bool logging;
    int currentDivisionOfBeat;
    std::string filename;

    // declare time units
    TimePoint startTime;
    Duration divisionDurationAsDuration;
    Duration timeCorrectionForBuffer;
    TimePoint nextDivisionTime;
    Duration processingDuration;

    // declare structures
    std::vector<std::pair<std::string, std::pair<TimePoint, TimePoint>>> processRecords;
    std::vector<TimePoint> divisionTimes;
    std::unordered_map<std::string, size_t> idTagToBatchIndex;
    std::deque<BatchActions> scheduledActionBatches;
    
    // declare threading mechanisms
    std::thread timerThread;
    mutable std::mutex timerMutex;
    std::mutex mtxNoteData;
    std::condition_variable bufferUpdateCV;
    std::condition_variable cvNoteData;
    mutable std::mutex scheduledIntervalsMutex; //
};

#endif // MASTER_CLOCK_H
