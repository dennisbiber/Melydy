#include "ScheduleAction.h"
#include <cstdio>

ScheduleAction::ScheduleAction(std::function<void()> function, bool verbose)
    : verbose(false), func(function) {}

void ScheduleAction::execute() const {
    if (func) {
        try {
            func(); // Execute the function
        } catch (const std::exception& ex) {
            printf("      ScheduleAction::execute::Exception caught: %s\n", ex.what());
        } catch (...) {
            printf("      ScheduleAction::execute::Unknown exception caught.\n");
        }
    } else {
        if (verbose) {
            printf("      ScheduleAction::execute::Invalid Function");
        }
    }
}
