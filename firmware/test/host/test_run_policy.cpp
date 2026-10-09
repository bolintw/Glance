// Host-side tests for the run decisions: retries, the offline screen, the
// battery thresholds. Build and run: `make -C firmware/test/host`.

#include <cstdio>

#include "run_policy.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

constexpr uint16_t pct(double p) { return static_cast<uint16_t>(p * 256); }

void testRetries() {
    using run_policy::retrySoon;
    CHECK(!retrySoon(0));  // it worked: the next daily refresh
    // 07:00 fails, 08:00 fails, 09:00 fails: tomorrow. And again the next day.
    CHECK(retrySoon(1));
    CHECK(retrySoon(2));
    CHECK(!retrySoon(3));
    CHECK(retrySoon(4));
    CHECK(retrySoon(5));
    CHECK(!retrySoon(6));
}

void testNextWake() {
    using run_policy::nextWake;
    // 2026-10-06 07:00:20 in Taiwan (23:00:20 UTC the day before).
    const int64_t morning = 1791327620;
    const int32_t taipei = 8 * 3600;
    auto ok = nextWake(0, morning, taipei, 7, 60, 3600);
    CHECK(!ok.retry && ok.at == morning - 20 + 24 * 3600);  // tomorrow 07:00
    auto retry = nextWake(1, morning, taipei, 7, 60, 3600);
    CHECK(retry.retry && retry.at == morning + 3600);
    auto giveUp = nextWake(3, morning + 2 * 3600, taipei, 7, 60, 3600);  // the 09:00 attempt failed too
    CHECK(!giveUp.retry && giveUp.at == morning - 20 + 24 * 3600);
}

void testResets() {
    using run_policy::ResetKind;
    using run_policy::classifyReset;
    CHECK(classifyReset(1) == ResetKind::normal);  // power-on
    CHECK(classifyReset(3) == ResetKind::normal);  // esp_restart
    CHECK(classifyReset(8) == ResetKind::normal);  // deep sleep
    CHECK(classifyReset(4) == ResetKind::crash);   // panic
    CHECK(classifyReset(6) == ResetKind::crash);   // task watchdog
    CHECK(classifyReset(15) == ResetKind::crash);  // CPU lock-up
    CHECK(classifyReset(9) == ResetKind::power);   // brownout
    CHECK(!run_policy::backOffAfterBadResets(2));
    CHECK(run_policy::backOffAfterBadResets(3));
}

void testWakePress() {
    using run_policy::WakePress;
    using run_policy::decideWakePress;
    const int64_t now = 1791327620;
    CHECK(decideWakePress(true, true, false, now + 3600, now) == WakePress::setup);
    CHECK(decideWakePress(true, true, true, now + 3600, now) == WakePress::setup);  // setup works offline too
    CHECK(decideWakePress(true, false, false, now + 3600, now) == WakePress::stuck);
    CHECK(decideWakePress(false, true, false, now + 3600, now) == WakePress::togglePrivacy);
    CHECK(decideWakePress(false, true, true, now + 3600, now) == WakePress::backToSleep);
    CHECK(decideWakePress(false, true, true, now + 30, now) == WakePress::refresh);  // due anyway
    CHECK(decideWakePress(false, true, true, 0, now) == WakePress::refresh);         // no timer was set
}

void testOfflineScreen() {
    using run_policy::offlineScreenDue;
    CHECK(!offlineScreenDue(false, false, 1));  // a router rebooting: keep the calendar
    CHECK(!offlineScreenDue(false, false, 2));
    CHECK(offlineScreenDue(false, false, 3));
    CHECK(offlineScreenDue(true, false, 1));  // right after setup
    CHECK(offlineScreenDue(false, true, 1));  // after a press
}

void testCalendarStale() {
    using run_policy::calendarStale;
    constexpr int64_t kDay = 24 * 3600;
    const int64_t monday7 = 1791327600;  // some 07:00
    CHECK(!calendarStale(0, monday7));  // not failing
    // Failed at Monday 07:00; Tuesday 07:00 fails too: still just one bad day.
    CHECK(!calendarStale(static_cast<uint32_t>(monday7), monday7 + kDay));
    // Wednesday 07:00, even waking a little early: two days, show it.
    CHECK(calendarStale(static_cast<uint32_t>(monday7), monday7 + 2 * kDay - 30 * 60));
    CHECK(calendarStale(static_cast<uint32_t>(monday7), monday7 + 3 * kDay));
}

void testBattery() {
    using run_policy::batteryLow;
    using run_policy::batteryTooLow;
    CHECK(batteryLow(pct(14.9)));
    CHECK(!batteryLow(pct(15)));

    // Going down: critical under 5%, or under 3.45V whatever the gauge says.
    CHECK(!batteryTooLow(pct(5), 3600, false));
    CHECK(batteryTooLow(pct(4.9), 3600, false));
    CHECK(batteryTooLow(pct(50), 3440, false));
    // Coming back: only from 10% and 3.55V, so it doesn't flap at 5%.
    CHECK(batteryTooLow(pct(9.9), 3700, true));
    CHECK(batteryTooLow(pct(12), 3540, true));
    CHECK(!batteryTooLow(pct(10), 3550, true));
    CHECK(!batteryTooLow(pct(7), 3600, false));  // between the two: fine while the reminder isn't up
    CHECK(batteryTooLow(pct(7), 3600, true));    // ...and still too low once it is
}

}  // namespace

int main() {
    testRetries();
    testNextWake();
    testResets();
    testWakePress();
    testOfflineScreen();
    testCalendarStale();
    testBattery();
    if (failures == 0) {
        std::printf("all run_policy tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
