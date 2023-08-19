#ifndef SCHEDULE_ACTION_H
#define SCHEDULE_ACTION_H

#include <functional>
#include <string>

class ScheduleAction {
public:
    explicit ScheduleAction(std::function<void()> function, bool verbose = false);
    void execute() const;

private:
    std::function<void()> func;
    bool verbose;
};

#endif // SCHEDULE_ACTION_H