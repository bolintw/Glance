#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "wake_log.hpp"

// What the firmware's parts share at run time: the state one boot hands the
// next, what this run is doing, and this run's wake log entry.
namespace glance {

inline constexpr const char* kTag = "main";
inline constexpr uint32_t kWifiConnectTimeoutMs = 15000;

// What one boot tells the next. Kept in RTC memory, which survives restarts
// and deep sleep but not power loss -- the magic tells a valid state from
// power-on garbage.
struct BootState {
    static constexpr uint32_t kMagic = 0x474C4E39;  // "GLN9" (bump when the layout changes)
    uint32_t magic;
    bool enterSetup;        // a long press asked for setup mode
    bool eraseAll;          // a press held to kEraseHoldMs asked to erase everything
    bool accessPointSetup;  // ...and it has to be the WiFi step on the access point
    bool wifiFailed;        // ...because the saved WiFi didn't connect (say so on the form)
    bool justConfigured;    // setup mode just saved; report WiFi trouble at once
    bool homeStepNext;      // the access point's WiFi step just saved: a first setup goes on
    bool pressed;           // a short press asked for this run: the screen has to change
    bool offlineShown;      // the panel shows no calendar (offline, or none came through): short presses don't switch modes
    bool batteryCritical;   // the charge reminder is up (see run_policy::kCriticalPct)
    uint8_t wifiFailures;   // runs in a row that couldn't get online
    uint8_t failedRuns;     // refreshes in a row that failed, for any reason
    uint8_t badResets;      // crashes and brownouts in a row (run_policy::classifyReset)
    uint8_t lastPhoto;  // privacy mode's last photo (see pickPhoto), so the next one differs
    int64_t wakeAt;     // when the timer is due (Unix time; 0 = none), for a button wake-up that goes back to sleep
    int8_t fakeBatteryPct;  // development: the next boot's state of charge instead of the gauge's (-1 = none)
    // The last forecast fetched, for privacy mode switched on by a press
    // (drawn offline; see run_policy::forecastFromToday). The drawing doesn't
    // use CWA's wording, so it isn't kept.
    int64_t forecastAt;          // when it was fetched (Unix time; 0 = none)
    uint8_t forecastCondition;   // weather::Condition
    int8_t forecastRainChance;   // percent
    int8_t forecastMinTemp;      // Celsius
    int8_t forecastMaxTemp;
};
extern BootState gBoot;

// Sets gBoot to its defaults unless it holds a valid state from before.
void initBootState();

// What the device is doing, for the button task to decide what a press means.
// In setup mode: setupStarting until its QR code is on the panel (presses are
// ignored: the screen is coming), setup while it's up (a long press leaves),
// setupClosed once a second device closed it (any press starts over).
enum class Phase { refreshing, waiting, setupStarting, setup, setupClosed };
extern std::atomic<Phase> gPhase;
// No calendars configured: a photo frame, always privacy mode's layout.
extern std::atomic<bool> gPhotoFrame;
// The battery is low (see run_policy::batteryLow): the status icon, no OTA.
extern bool gLowBattery;

// This run, for the wake log (see components/wake_log): filled in as it goes,
// stored just before sleeping.
extern wake_log::Entry gRun;
void logRun(wake_log::Result result);

[[noreturn]] void restartInto(bool enterSetup);
// Restarts into erasing everything (see eraseAndShip): from a clean boot, so
// nothing a run was in the middle of gets in the way.
[[noreturn]] void restartToErase();

// Deep sleep until `wakeAt` (Unix time) or a press of the button, whichever
// comes first; with no `wakeAt`, until the button only. The button wakes
// through ext1, which lets the RTC peripherals power down; `buttonOnExt0`
// (development, for comparing sleep currents) uses ext0 instead, which
// keeps them powered.
[[noreturn]] void deepSleep(std::optional<int64_t> wakeAt, bool buttonOnExt0 = false);

}  // namespace glance
