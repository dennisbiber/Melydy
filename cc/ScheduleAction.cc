#include "ScheduleAction.h"
#include <cstdio>

// Constructor for functions that don't need time correction
ScheduleAction::ScheduleAction(std::function<void()> function, bool verbose)
    : verbose(verbose), func(function), hasTimeCorrection(false), timeCorrection(0) {}

// Constructor for functions that need time correction
ScheduleAction::ScheduleAction(std::function<void(Duration)> function, bool verbose, Duration timeCorrection)
    : verbose(verbose), funcWithTimeCorrection(function), hasTimeCorrection(true), timeCorrection(timeCorrection) {
}

void ScheduleAction::setTimeCorrection(Duration timeCorrectionValue) {
    hasTimeCorrection = true;
    timeCorrection = timeCorrectionValue;
}

void ScheduleAction::setTimeCorrectionFlagFalse() {
    hasTimeCorrection = false;
}

void ScheduleAction::execute() const {
    if (hasTimeCorrection && funcWithTimeCorrection) {
        try {
            funcWithTimeCorrection(timeCorrection);
        } catch (const std::exception& ex) {
            printf("Exception caught: %s\n", ex.what());
        } catch (...) {
            printf("Unknown exception caught.\n");
        }
    } else if (func) {
        try {
            func();
        } catch (const std::exception& ex) {
            printf("Exception caught: %s\n", ex.what());
        } catch (...) {
            printf("Unknown exception caught.\n");
        }
    } else {
        if (verbose) {
            printf("Invalid Function");
        }
    }
}
