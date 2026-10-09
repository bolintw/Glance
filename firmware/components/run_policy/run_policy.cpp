#include "run_policy.hpp"

#include "refresh_schedule.hpp"

namespace run_policy {

bool retrySoon(uint8_t failedRunsInARow) { return failedRunsInARow % kAttemptsInARow != 0; }

NextWake nextWake(uint8_t failedRunsInARow, int64_t now, int32_t utcOffset, int refreshHour, int retryMinutes,
                  int64_t minGapSeconds) {
    if (retrySoon(failedRunsInARow)) {
        return {now + static_cast<int64_t>(retryMinutes) * 60, true};
    }
    return {refresh_schedule::nextDaily(now, utcOffset, refreshHour, 0, minGapSeconds), false};
}

ResetKind classifyReset(uint8_t resetReason) {
    switch (resetReason) {
        case 4:   // ESP_RST_PANIC
        case 5:   // ESP_RST_INT_WDT
        case 6:   // ESP_RST_TASK_WDT
        case 7:   // ESP_RST_WDT
        case 15:  // ESP_RST_CPU_LOCKUP
            return ResetKind::crash;
        case 9:   // ESP_RST_BROWNOUT
        case 14:  // ESP_RST_PWR_GLITCH
            return ResetKind::power;
        default:
            return ResetKind::normal;
    }
}

bool backOffAfterBadResets(uint8_t badResetsInARow) { return badResetsInARow >= kBadResetsBeforeBackoff; }

WakePress decideWakePress(bool heldToLongPress, bool released, bool noCalendarToSwitchTo, int64_t timerDueAt,
                          int64_t now) {
    if (heldToLongPress) {
        return released ? WakePress::setup : WakePress::stuck;
    }
    if (!noCalendarToSwitchTo) {
        return WakePress::togglePrivacy;
    }
    return timerDueAt > now + 60 ? WakePress::backToSleep : WakePress::refresh;
}

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
