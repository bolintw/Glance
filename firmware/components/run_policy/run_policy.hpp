#pragma once

#include <cstdint>

// The decisions a run makes from counts and readings -- when to retry, when
// the offline screen goes up, how low the battery may go -- kept free of
// ESP-IDF so they're tested on the host. main/ does the acting.
namespace run_policy {

// Consecutive failures to get online (an hour apart) before the calendar
// gives way to the offline screen. A router rebooting at 07:00 shouldn't
// wipe the calendar off the screen.
inline constexpr uint8_t kOfflineRunsBeforeScreen = 3;

// A failed refresh is retried hourly (CONFIG_GLANCE_RETRY_MINUTES), up to
// this many attempts in a row -- 07:00, 08:00, 09:00 -- then it waits for
// the next daily refresh: retrying all day would cost ten normal days' worth
// of battery.
inline constexpr uint8_t kAttemptsInARow = 3;

// Battery thresholds: state of charge from the fuel gauge, voltage at rest
// (read before WiFi starts). Low: a status icon, and no OTA downloads.
// Critical: the charge reminder, then just an hourly look at the gauge
// until it's back past the resume levels -- the gap keeps it from flapping.
// The voltage guard is for worn cells whose voltage gives out before the
// gauge's estimate does.
inline constexpr int kLowBatteryPct = 15;
inline constexpr int kCriticalPct = 5;
inline constexpr uint16_t kCriticalMillivolts = 3450;
inline constexpr int kResumePct = 10;
inline constexpr uint16_t kResumeMillivolts = 3550;

// Whether the next wake-up is a retry (an hour on) rather than the next
// daily refresh, after `failedRunsInARow` failed refreshes (0 after one that
// worked).
bool retrySoon(uint8_t failedRunsInARow);

// Whether a calendar run that couldn't get online should put up the offline
// screen: right after setup (most likely a mistyped password), after a press
// (it has to change something), or once the failures pile up.
bool offlineScreenDue(bool justConfigured, bool pressed, uint8_t offlineRunsInARow);

// The battery is low: the status icon, and no OTA downloads.
bool batteryLow(uint16_t soc256);

// The battery is too low to run at all, with the gap between the critical
// and resume levels depending on whether the charge reminder is up.
bool batteryTooLow(uint16_t soc256, uint16_t millivolts, bool reminderUp);

}  // namespace run_policy
