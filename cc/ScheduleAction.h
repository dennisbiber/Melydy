#ifndef SCHEDULE_ACTION_H
#define SCHEDULE_ACTION_H

#include <functional>
#include <string>
#include <chrono> // Include the necessary header for Duration

using Duration = std::chrono::high_resolution_clock::duration;

class ScheduleAction {
public:
    explicit ScheduleAction(std::function<void()> function, bool verbose = false);
    explicit ScheduleAction(std::function<void(Duration)> funcWithDuration,
                            bool verbose = false, Duration timeCorrection = Duration(0));
    void execute() const;
    void setTimeCorrection(Duration timeCorrectionValue);
    void setTimeCorrectionFlagFalse();

private:
    std::function<void()> func;
    std::function<void(Duration)> funcWithTimeCorrection;
    bool verbose;
    bool hasTimeCorrection;
    Duration timeCorrection;
};

#endif // SCHEDULE_ACTION_H
