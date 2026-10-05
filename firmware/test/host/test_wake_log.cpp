// Host-side tests for the wake log format and the MAX17048 conversions.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <string>
#include <vector>

#include "max17048.hpp"
#include "wake_log.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using wake_log::Entry;

Entry sample(uint32_t seq) {
    Entry e;
    e.seq = seq;
    e.time = 1791244800;  // 2026-10-06 00:00 UTC
    e.millivolts = 3987;
    e.soc256 = 78 * 256 + 128;
    e.crate = -5;
    e.wifiMs = 1234;
    e.awakeMs = 17890;
    e.clockDriftMs = -420;
    e.rssi = -61;
    e.wake = wake_log::Wake::timer;
    e.result = wake_log::Result::privacy;
    e.resetReason = 8;
    return e;
}

// A region of `count` erased entries, with `entries[i]` written at `start + i`.
std::vector<uint8_t> region(size_t count, size_t start, const std::vector<Entry>& entries) {
    std::vector<uint8_t> bytes(count * wake_log::kEntryBytes, 0xFF);
    for (size_t i = 0; i < entries.size(); i++) {
        auto encoded = wake_log::encode(entries[i]);
        std::copy(encoded.begin(), encoded.end(), bytes.begin() + ((start + i) % count) * wake_log::kEntryBytes);
    }
    return bytes;
}

void testRoundTrip() {
    auto bytes = wake_log::encode(sample(7));
    auto back = wake_log::decode(bytes);
    CHECK(back.has_value());
    CHECK(back->seq == 7 && back->time == 1791244800 && back->millivolts == 3987);
    CHECK(back->soc256 == 78 * 256 + 128 && back->crate == -5 && back->wifiMs == 1234);
    CHECK(back->awakeMs == 17890 && back->clockDriftMs == -420 && back->rssi == -61);
    CHECK(back->wake == wake_log::Wake::timer && back->result == wake_log::Result::privacy && back->resetReason == 8);
}

void testBadEntries() {
    std::array<uint8_t, wake_log::kEntryBytes> erased;
    erased.fill(0xFF);
    CHECK(!wake_log::decode(erased));
    auto torn = wake_log::encode(sample(3));
    torn[9] ^= 0x10;
    CHECK(!wake_log::decode(torn));
    auto swapped = wake_log::encode(sample(3));
    std::swap(swapped[8], swapped[9]);
    CHECK(!wake_log::decode(swapped));
}

void testPositions() {
    auto empty = region(8, 0, {});
    auto first = wake_log::nextPosition(empty);
    CHECK(first.index == 0 && first.seq == 1);
    CHECK(wake_log::entriesInOrder(empty).empty());

    auto some = region(8, 0, {sample(1), sample(2), sample(3)});
    auto next = wake_log::nextPosition(some);
    CHECK(next.index == 3 && next.seq == 4);

    // Wrapped: seq 10..17 fill the ring starting at index 5; the newest
    // (17) sits at index 4, so 18 goes to 5.
    std::vector<Entry> wrapped;
    for (uint32_t s = 10; s <= 17; s++) {
        wrapped.push_back(sample(s));
    }
    auto ring = region(8, 5, wrapped);
    auto after = wake_log::nextPosition(ring);
    CHECK(after.index == 5 && after.seq == 18);
    auto ordered = wake_log::entriesInOrder(ring);
    CHECK(ordered.size() == 8 && ordered.front().seq == 10 && ordered.back().seq == 17);
    bool ascending = true;
    for (size_t i = 1; i < ordered.size(); i++) {
        ascending = ascending && ordered[i].seq == ordered[i - 1].seq + 1;
    }
    CHECK(ascending);

    // A sector erased ahead of the head (entries 5-7 gone) changes nothing.
    std::fill(ring.begin() + 5 * wake_log::kEntryBytes, ring.end(), 0xFF);
    CHECK(wake_log::nextPosition(ring).index == 5);
}

void testCsv() {
    Entry e = sample(1);
    Entry unknown;  // no clock, no battery, no WiFi
    unknown.seq = 2;
    unknown.wake = wake_log::Wake::powerOn;
    unknown.result = wake_log::Result::setup;
    std::vector<Entry> entries = {e, unknown};
    std::string csv = wake_log::toCsv(entries, 8 * 3600);
    CHECK(csv.starts_with("seq,time,wake,result,"));
    CHECK(csv.find("\n1,2026-10-06 08:00:00,timer,privacy,8,17.9,3.987,78.50,-1.0,1.23,-61,-420\n") !=
          std::string::npos);
    CHECK(csv.find("\n2,,power-on,setup,0,0.0,,,,,,\n") != std::string::npos);
}

void testMax17048() {
    CHECK(max17048::millivolts(0) == 0);
    CHECK(max17048::millivolts(0xC800) == 4000);  // 51200 * 78.125 uV
    CHECK(max17048::millivolts(0xAF00) == 3500);  // 44800 * 78.125 uV
    CHECK(max17048::millivolts(0xB333) == 3584);  // rounds to nearest
    CHECK(max17048::isVersion(0x0012));
    CHECK(!max17048::isVersion(0xFFFF));
    CHECK(!max17048::isVersion(0x0000));
}

}  // namespace

int main() {
    testRoundTrip();
    testBadEntries();
    testPositions();
    testCsv();
    testMax17048();
    if (failures == 0) {
        std::printf("all wake_log tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
