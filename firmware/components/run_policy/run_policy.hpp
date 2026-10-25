#pragma once

#include <cstdint>
#include <span>

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
// gauge's estimate does. Low enough to use most of the cell: the device
// takes about 0.8 mAh a day, so 1% of a 1000 mAh cell lasts some 12 days --
// the icon gives about two months' notice, and 2% (about 3.5V at rest,
// measured on V3.0) leaves weeks of sleep after the reminder.
inline constexpr int kLowBatteryPct = 5;
inline constexpr int kCriticalPct = 2;
inline constexpr uint16_t kCriticalMillivolts = 3450;
inline constexpr int kResumePct = 4;
inline constexpr uint16_t kResumeMillivolts = 3550;

// A calendar that has failed this long -- two daily refreshes in a row (an
// hour's slack for the wake-up time) -- gets the calendar-sync icon. One bad
// morning doesn't.
inline constexpr int64_t kCalendarStaleSeconds = 47 * 3600;

// Which forecast a refresh at `now` shows, as a moment in its 12-hour
// period: today's daytime (noon) before 12:00, tomorrow's from then on. The
// panel keeps it until the next refresh, often the next morning, so an
// evening refresh says whether tomorrow needs an umbrella.
int64_t forecastFor(int64_t now, int32_t utcOffset);

// Privacy mode switched on by a press is drawn without going online: the
// clock (kept by the RTC across deep sleep) gives the date, and the weather
// is the forecast fetched earlier -- as long as it's for today or later
// (see forecastFor), none if it's older.
bool forecastStillCurrent(int64_t fetchedAt, int64_t now, int32_t utcOffset);

// Whether the next wake-up is a retry (an hour on) rather than the next
// daily refresh, after `failedRunsInARow` failed refreshes (0 after one that
// worked).
bool retrySoon(uint8_t failedRunsInARow);

// When to wake next after `failedRunsInARow` failed runs: the next daily
// refresh at one of `refreshMinutes` (minutes after local midnight,
// `utcOffset`), or `retryMinutes` on for a retry. A failed run may not even have a synced clock, so a retry is a
// plain delay rather than a time of day. `minGapSeconds`: a wake-up this
// close before the daily time counts as that refresh (see
// refresh_schedule::nextDaily).
struct NextWake {
    int64_t at;
    bool retry;
};
NextWake nextWake(uint8_t failedRunsInARow, int64_t now, int32_t utcOffset, std::span<const int> refreshMinutes,
                  int retryMinutes, int64_t minGapSeconds);

// Why the chip last reset (esp_reset_reason_t), as far as runs care: a crash
// (panic, a watchdog, a CPU lock-up), the power (a brownout or a glitch), or
// anything normal (power-on, a restart, waking from deep sleep).
enum class ResetKind { normal, crash, power };
ResetKind classifyReset(uint8_t resetReason);

// Crashes and brownouts in a row before the device stops trying for a while:
// each one reboots straight away, so a fault that keeps recurring would
// otherwise reboot it nonstop and flatten the battery.
inline constexpr uint8_t kBadResetsBeforeBackoff = 3;
bool backOffAfterBadResets(uint8_t badResetsInARow);

// Whether calendars and the weather are fetched at once (refresh.cpp) or one
// after another, the way that has been proven longest. At once draws more
// current at its peak -- both cores in TLS handshakes -- so not on a battery
// below kParallelFetchPct (or with no reading), where a sagging cell is
// likelier to brown out; and not right after a crash or a brownout, in case
// it's what keeps bringing the device down.
inline constexpr int kParallelFetchPct = 20;
bool fetchInParallel(uint16_t soc256, ResetKind lastReset);

// What a press that woke the device from deep sleep means, timed from the
// wake-up: held to the long-press time and let go, setup mode -- or, held to
// the erase time (see glance::kEraseHoldMs) first, erase everything; still
// down at the stuck time, a stuck button -- carry on as if it weren't pressed;
// a short press toggles privacy mode, unless there's no calendar to switch
// to (offline, photo frame): then straight back to sleep until the timer
// was due -- or the refresh, if that's within a minute anyway.
enum class WakePress { setup, erase, stuck, togglePrivacy, backToSleep, refresh };
WakePress decideWakePress(bool heldToLongPress, bool released, bool heldToErase, bool noCalendarToSwitchTo,
                          int64_t timerDueAt, int64_t now);

// Whether a calendar run that couldn't get online should put up the offline
// screen: right after setup (most likely a mistyped password), after a press
// (it has to change something), or once the failures pile up.
bool offlineScreenDue(bool justConfigured, bool pressed, uint8_t offlineRunsInARow);

// Whether a calendar failing since `failingSince` (Unix time; 0 = it isn't)
// has failed long enough to show.
bool calendarStale(uint32_t failingSince, int64_t now);

// The battery is low: the status icon, and no OTA downloads.
bool batteryLow(uint16_t soc256);

// The battery is too low to run at all, with the gap between the critical
// and resume levels depending on whether the charge reminder is up.
bool batteryTooLow(uint16_t soc256, uint16_t millivolts, bool reminderUp);

}  // namespace run_policy
