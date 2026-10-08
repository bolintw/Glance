#include "wake_log.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace wake_log {
namespace {

constexpr uint8_t kCheckSeed = 0xA5;

uint8_t checkByte(std::span<const uint8_t> bytes) {
    uint8_t check = kCheckSeed;
    for (uint8_t b : bytes) {
        check = static_cast<uint8_t>((check << 1 | check >> 7) ^ b);  // rotate-xor: catches swapped bytes too
    }
    return check;
}

void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
    put16(p, static_cast<uint16_t>(v));
    put16(p + 2, static_cast<uint16_t>(v >> 16));
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t get32(const uint8_t* p) { return get16(p) | static_cast<uint32_t>(get16(p + 2)) << 16; }

const char* wakeName(Wake wake) {
    switch (wake) {
        case Wake::powerOn: return "power-on";
        case Wake::timer: return "timer";
        case Wake::button: return "button";
        case Wake::other: return "other";
    }
    return "?";
}

const char* resultName(Result result) {
    switch (result) {
        case Result::calendar: return "calendar";
        case Result::privacy: return "privacy";
        case Result::offlineShown: return "offline-shown";
        case Result::offlineKept: return "offline-kept";
        case Result::fetchFailed: return "fetch-failed";
        case Result::panelFailed: return "panel-failed";
        case Result::backToSleep: return "back-to-sleep";
        case Result::setup: return "setup";
        case Result::chargeReminder: return "charge-reminder";
        case Result::stillLow: return "still-low";
        case Result::calendarError: return "calendar-error";
    }
    return "?";
}

}  // namespace

std::array<uint8_t, kEntryBytes> encode(const Entry& e) {
    std::array<uint8_t, kEntryBytes> out{};
    uint8_t* p = out.data();
    put32(p + 0, e.seq);
    put32(p + 4, e.time);
    put16(p + 8, e.millivolts);
    put16(p + 10, e.soc256);
    put16(p + 12, static_cast<uint16_t>(e.crate));
    put16(p + 14, e.wifiMs);
    put32(p + 16, e.awakeMs);
    put32(p + 20, static_cast<uint32_t>(e.clockDriftMs));
    p[24] = static_cast<uint8_t>(e.rssi);
    p[25] = static_cast<uint8_t>(e.wake);
    p[26] = static_cast<uint8_t>(e.result);
    p[27] = e.resetReason;
    // 28-30 reserved (zero)
    out[31] = checkByte(std::span(out).first(31));
    return out;
}

std::optional<Entry> decode(std::span<const uint8_t, kEntryBytes> bytes) {
    const uint8_t* p = bytes.data();
    if (get32(p) == kEmptySeq || bytes[31] != checkByte(bytes.first(31))) {
        return std::nullopt;
    }
    Entry e;
    e.seq = get32(p + 0);
    e.time = get32(p + 4);
    e.millivolts = get16(p + 8);
    e.soc256 = get16(p + 10);
    e.crate = static_cast<int16_t>(get16(p + 12));
    e.wifiMs = get16(p + 14);
    e.awakeMs = get32(p + 16);
    e.clockDriftMs = static_cast<int32_t>(get32(p + 20));
    e.rssi = static_cast<int8_t>(p[24]);
    e.wake = static_cast<Wake>(p[25]);
    e.result = static_cast<Result>(p[26]);
    e.resetReason = p[27];
    return e;
}

namespace {
std::optional<Entry> at(std::span<const uint8_t> region, size_t index) {
    return decode(region.subspan(index * kEntryBytes).first<kEntryBytes>());
}
}  // namespace

Position nextPosition(std::span<const uint8_t> region) {
    size_t count = region.size() / kEntryBytes;
    std::optional<size_t> newest;
    uint32_t newestSeq = 0;
    for (size_t i = 0; i < count; i++) {
        if (auto e = at(region, i); e && e->seq >= newestSeq) {
            newest = i;
            newestSeq = e->seq;
        }
    }
    if (!newest) {
        return {0, 1};
    }
    return {(*newest + 1) % count, newestSeq + 1};
}

void forEachInOrder(std::span<const uint8_t> region, const std::function<void(const Entry&)>& visit) {
    size_t count = region.size() / kEntryBytes;
    size_t oldest = nextPosition(region).index;
    for (size_t n = 0; n < count; n++) {
        if (auto e = at(region, (oldest + n) % count)) {
            visit(*e);
        }
    }
}

std::vector<Entry> entriesInOrder(std::span<const uint8_t> region) {
    std::vector<Entry> entries;
    forEachInOrder(region, [&](const Entry& e) { entries.push_back(e); });
    return entries;
}

void HealthTally::add(const Entry& e) {
    if (e.result == Result::calendar || e.result == Result::privacy) {
        health_.lastGoodRefresh = std::max(health_.lastGoodRefresh, e.time);
    }
    if (e.millivolts != 0) {
        health_.latestBattery = e;
    }
    if (windowStart_ != 0 && (e.time == 0 || e.time < windowStart_)) {
        return;
    }
    switch (e.resetReason) {  // esp_reset_reason_t
        case 4:   // ESP_RST_PANIC
        case 5:   // ESP_RST_INT_WDT
        case 6:   // ESP_RST_TASK_WDT
        case 7:   // ESP_RST_WDT
        case 15:  // ESP_RST_CPU_LOCKUP
            health_.crashes++;
            break;
        case 9:   // ESP_RST_BROWNOUT
        case 14:  // ESP_RST_PWR_GLITCH
            health_.brownouts++;
            break;
        default:
            break;
    }
    switch (e.result) {
        case Result::offlineShown:
        case Result::offlineKept:
        case Result::fetchFailed:
        case Result::panelFailed:
        case Result::calendarError:
            health_.failedRuns++;
            break;
        default:
            break;
    }
    if (e.millivolts > kHighMillivolts) {
        health_.highVoltage++;
        health_.highestMillivolts = std::max(health_.highestMillivolts, e.millivolts);
    }
}

std::string csvHeader() {
    return "seq,time,wake,result,reset_reason,awake_s,battery_v,soc_pct,charge_rate_pct_per_h,wifi_s,rssi_dbm,"
           "clock_drift_ms\n";
}

std::string csvLine(const Entry& e, int32_t utcOffset) {
    char when[20] = "";
    if (e.time != 0) {
        time_t local = static_cast<time_t>(e.time) + utcOffset;
        struct tm t;
        gmtime_r(&local, &t);
        std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &t);
    }
    char wifi[12] = "";
    if (e.wifiMs != kNoWifi) {
        std::snprintf(wifi, sizeof(wifi), "%.2f", e.wifiMs / 1000.0);
    }
    char drift[16] = "";
    if (e.clockDriftMs != kNoDrift) {
        std::snprintf(drift, sizeof(drift), "%ld", static_cast<long>(e.clockDriftMs));
    }
    char battery[32] = ",,";
    if (e.millivolts != 0) {
        std::snprintf(battery, sizeof(battery), "%.3f,%.2f,%.1f", e.millivolts / 1000.0, e.soc256 / 256.0,
                      e.crate * 0.208);
    }
    char line[192];
    std::snprintf(line, sizeof(line), "%lu,%s,%s,%s,%u,%.1f,%s,%s,%s,%s\n", static_cast<unsigned long>(e.seq),
                  when, wakeName(e.wake), resultName(e.result), e.resetReason, e.awakeMs / 1000.0, battery, wifi,
                  e.rssi ? std::to_string(e.rssi).c_str() : "", drift);
    return line;
}

std::string toCsv(std::span<const Entry> entries, int32_t utcOffset) {
    std::string csv = csvHeader();
    for (const Entry& e : entries) {
        csv += csvLine(e, utcOffset);
    }
    return csv;
}

}  // namespace wake_log
