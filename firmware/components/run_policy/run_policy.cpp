#include "run_policy.hpp"

namespace run_policy {

bool retrySoon(uint8_t failedRunsInARow) { return failedRunsInARow % kAttemptsInARow != 0; }

bool offlineScreenDue(bool justConfigured, bool pressed, uint8_t offlineRunsInARow) {
    return justConfigured || pressed || offlineRunsInARow >= kOfflineRunsBeforeScreen;
}

bool calendarStale(uint32_t failingSince, int64_t now) {
    return failingSince != 0 && now - static_cast<int64_t>(failingSince) >= kCalendarStaleSeconds;
}

bool batteryLow(uint16_t soc256) { return soc256 < kLowBatteryPct * 256; }

bool batteryTooLow(uint16_t soc256, uint16_t millivolts, bool reminderUp) {
    int pct = soc256 / 256;
    if (reminderUp) {
        return pct < kResumePct || millivolts < kResumeMillivolts;
    }
    return pct < kCriticalPct || millivolts < kCriticalMillivolts;
}

}  // namespace run_policy
