#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

// One record per wake-up -- battery, what the run did, how long it took --
// kept on the device so battery life can be worked out from a week or two
// on battery, read back without plugging in USB (which would charge it).
// Pure C++ (tested on the host): the encoding, finding where the ring
// continues, and the CSV. The flash side is components/wake_log_store.
namespace wake_log {

enum class Wake : uint8_t { powerOn, timer, button, other };

enum class Result : uint8_t {
    calendar,      // the calendar was drawn
    privacy,       // privacy mode (or the photo frame) was drawn, online
    offlineShown,  // couldn't get online; the offline screen is up
    offlineKept,   // couldn't get online; the last calendar stays up
    fetchFailed,   // online, but no calendar could be fetched
    panelFailed,   // the panel didn't respond
    backToSleep,   // a button wake-up with nothing to do (offline, photo frame)
    setup,         // went into setup mode
    chargeReminder,  // the battery ran too low: "please charge" went up
    stillLow,        // checked the battery while the reminder is up: still too low
    calendarError,   // online, no calendar for two days: the photo layout with the calendar icon
    stuck,           // the run went past its deadline and was cut short (main.cpp, kRunDeadlineMs)
    badResetBackoff,  // crashes or brownouts kept recurring: sleeping a while before trying again
};

// Where the power came from, on boards that can tell (V3.0's USB and charge
// status inputs); unknown on V2.1 and in entries from before.
enum class Power : uint8_t {
    unknown,
    battery,   // no USB
    charging,  // USB in, the charger charging
    usb,       // USB in, not charging (charged, or no battery)
};

inline constexpr uint16_t kNoWifi = 0xFFFF;           // wifiMs: not tried
inline constexpr int32_t kNoDrift = INT32_MIN;        // clockDriftMs: no clock to compare
inline constexpr uint32_t kEmptySeq = 0xFFFFFFFF;     // erased flash

struct Entry {
    uint32_t seq = 0;          // 1, 2, ... (filled in when stored)
    uint32_t time = 0;         // Unix time at the end of the run; 0 if the clock was unknown
    uint16_t millivolts = 0;   // battery, read before WiFi starts; 0 = no reading
    uint16_t soc256 = 0;       // state of charge, % * 256
    int16_t crate = 0;         // charge rate, MAX17048 units (0.208 %/h)
    uint16_t wifiMs = kNoWifi; // time to connect
    uint32_t awakeMs = 0;      // from boot to going back to sleep
    int32_t clockDriftMs = kNoDrift;  // how far NTP moved the clock (positive: it was slow)
    int8_t rssi = 0;           // dBm; 0 = not connected
    Wake wake = Wake::other;
    Result result = Result::calendar;
    uint8_t resetReason = 0;   // esp_reset_reason_t
    Power power = Power::unknown;
};

inline constexpr size_t kEntryBytes = 32;

std::array<uint8_t, kEntryBytes> encode(const Entry& entry);
// nullopt for an erased slot or a torn/corrupt one.
std::optional<Entry> decode(std::span<const uint8_t, kEntryBytes> bytes);

// Where the next entry goes in `region` (a whole number of entries, used as
// a ring): after the one with the highest seq.
struct Position {
    size_t index;
    uint32_t seq;
};
Position nextPosition(std::span<const uint8_t> region);

// The valid entries, oldest first: the ring from the slot after the newest.
void forEachInOrder(std::span<const uint8_t> region, const std::function<void(const Entry&)>& visit);
std::vector<Entry> entriesInOrder(std::span<const uint8_t> region);

// What the log says about the device's health, for the setup page.
inline constexpr uint16_t kHighMillivolts = 4300;  // a Li-ion charger stops at 4.2V
struct Health {
    uint32_t lastGoodRefresh = 0;        // newest calendar or online privacy run (Unix time; 0 = none)
    std::optional<Entry> latestBattery;  // newest entry with a battery reading
    // Counted within the window only:
    int crashes = 0;      // runs that started after a panic, a watchdog or a CPU lock-up
    int brownouts = 0;    // ...after a brownout or a power glitch
    int failedRuns = 0;   // refreshes that didn't get the calendar or photo up, retries included
    int highVoltage = 0;  // battery readings over kHighMillivolts (charging gone wrong)
    uint16_t highestMillivolts = 0;
};

// Builds a Health from the entries, oldest first. `windowStart` (Unix time)
// limits the counts to recent runs; 0 counts everything.
class HealthTally {
public:
    explicit HealthTally(uint32_t windowStart) : windowStart_(windowStart) {}
    void add(const Entry& entry);
    const Health& health() const { return health_; }

private:
    uint32_t windowStart_;
    Health health_;
};

// CSV, a line at a time (each ends in "\n"); times in local time (`utcOffset`).
std::string csvHeader();
std::string csvLine(const Entry& entry, int32_t utcOffset);
std::string toCsv(std::span<const Entry> entries, int32_t utcOffset);

}  // namespace wake_log
