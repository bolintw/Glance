// Host-side tests for refresh_schedule::nextDaily.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>

#include "ics_datetime.hpp"
#include "refresh_schedule.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

constexpr int32_t kTaipei = 8 * 3600;
constexpr int64_t kHour = 3600;

// Unix time of a Taipei wall-clock moment.
int64_t taipei(int y, int m, int d, int hh, int mm) {
    return ics::daysFromCivil({y, m, d}) * 86400 + hh * kHour + mm * 60 - kTaipei;
}

int64_t next7am(int64_t now) { return refresh_schedule::nextDaily(now, kTaipei, 7, 0, kHour); }

void testSameDayAndNextDay() {
    // Evening and after-midnight runs both target the coming 07:00.
    CHECK(next7am(taipei(2026, 10, 4, 19, 30)) == taipei(2026, 10, 5, 7, 0));
    CHECK(next7am(taipei(2026, 10, 5, 0, 10)) == taipei(2026, 10, 5, 7, 0));
    CHECK(next7am(taipei(2026, 10, 5, 5, 59)) == taipei(2026, 10, 5, 7, 0));
    // The scheduled run itself, and any time after it, books tomorrow.
    CHECK(next7am(taipei(2026, 10, 5, 7, 0)) == taipei(2026, 10, 6, 7, 0));
    CHECK(next7am(taipei(2026, 10, 5, 7, 1)) == taipei(2026, 10, 6, 7, 0));
    CHECK(next7am(taipei(2026, 10, 5, 23, 59)) == taipei(2026, 10, 6, 7, 0));
}

void testEarlyWakeDoesNotRefreshTwice() {
    // Waking a bit before 07:00 counts as this morning's refresh.
    CHECK(next7am(taipei(2026, 10, 5, 6, 58)) == taipei(2026, 10, 6, 7, 0));
    CHECK(next7am(taipei(2026, 10, 5, 6, 1)) == taipei(2026, 10, 6, 7, 0));
    // Exactly one gap before is still too close.
    CHECK(next7am(taipei(2026, 10, 5, 6, 0)) == taipei(2026, 10, 6, 7, 0));
}

void testUtcDayBoundary() {
    // 07:00 Taipei is 23:00 UTC the previous day; the local date must win.
    int64_t now = taipei(2026, 10, 5, 7, 30);  // 2026-10-04 23:30 UTC
    CHECK(next7am(now) == taipei(2026, 10, 6, 7, 0));
    CHECK(refresh_schedule::nextDaily(taipei(2026, 10, 5, 7, 30), 0, 7, 0, kHour) ==
          ics::daysFromCivil({2026, 10, 5}) * 86400 + 7 * kHour);
}

void testMonthAndYearRollover() {
    CHECK(next7am(taipei(2026, 10, 31, 20, 0)) == taipei(2026, 11, 1, 7, 0));
    CHECK(next7am(taipei(2026, 12, 31, 8, 0)) == taipei(2027, 1, 1, 7, 0));
}

void testOtherTimes() {
    CHECK(refresh_schedule::nextDaily(taipei(2026, 10, 5, 12, 0), kTaipei, 18, 30, kHour) ==
          taipei(2026, 10, 5, 18, 30));
    CHECK(refresh_schedule::nextDaily(taipei(2026, 10, 5, 12, 0), kTaipei, 0, 0, kHour) ==
          taipei(2026, 10, 6, 0, 0));
}

}  // namespace

int main() {
    testSameDayAndNextDay();
    testEarlyWakeDoesNotRefreshTwice();
    testUtcDayBoundary();
    testMonthAndYearRollover();
    testOtherTimes();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all refresh_schedule tests passed\n");
    return 0;
}
