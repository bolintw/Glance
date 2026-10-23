#pragma once

#include <cstdint>
#include <span>

// When to wake up next. Pure arithmetic on Unix seconds and a fixed UTC
// offset (no DST, same as the rest of the firmware), so it's host-testable.
namespace refresh_schedule {

// The first `hour`:`minute` local time that is more than `minGapSeconds`
// after `now`. The gap keeps a wake-up that lands a little before the
// scheduled time from booking a second refresh minutes later.
int64_t nextDaily(int64_t now, int32_t utcOffset, int hour, int minute, int64_t minGapSeconds);

// The soonest of nextDaily for each of `minutesOfDay` (minutes after local
// midnight; at least one).
int64_t nextOf(int64_t now, int32_t utcOffset, std::span<const int> minutesOfDay, int64_t minGapSeconds);

}  // namespace refresh_schedule
