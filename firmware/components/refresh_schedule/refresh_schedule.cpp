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

}  // namespace refresh_schedule
