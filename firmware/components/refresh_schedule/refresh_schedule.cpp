#include "refresh_schedule.hpp"

namespace refresh_schedule {
namespace {
constexpr int64_t kSecondsPerDay = 24 * 3600;

int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}
}  // namespace

int64_t nextDaily(int64_t now, int32_t utcOffset, int hour, int minute, int64_t minGapSeconds) {
    int64_t localMidnight = floorDiv(now + utcOffset, kSecondsPerDay) * kSecondsPerDay;
    int64_t next = localMidnight + hour * 3600 + minute * 60 - utcOffset;
    while (next <= now + minGapSeconds) {
        next += kSecondsPerDay;
    }
    return next;
}

int64_t nextOf(int64_t now, int32_t utcOffset, std::span<const int> minutesOfDay, int64_t minGapSeconds) {
    int64_t soonest = INT64_MAX;
    for (int minutes : minutesOfDay) {
        int64_t next = nextDaily(now, utcOffset, minutes / 60, minutes % 60, minGapSeconds);
        soonest = next < soonest ? next : soonest;
    }
    return soonest;
}

}  // namespace refresh_schedule
