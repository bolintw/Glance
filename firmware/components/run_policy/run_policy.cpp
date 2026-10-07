#include "run_policy.hpp"

namespace run_policy {

bool retrySoon(uint8_t failedRunsInARow) { return failedRunsInARow % kAttemptsInARow != 0; }

bool offlineScreenDue(bool justConfigured, bool pressed, uint8_t offlineRunsInARow) {
    return justConfigured || pressed || offlineRunsInARow >= kOfflineRunsBeforeScreen;
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
