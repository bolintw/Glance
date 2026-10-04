#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// Calendar math done by hand rather than through libc's mktime/localtime:
// those depend on the process-global TZ, which differs between the board
// and the host tests, and the expander needs to work in several zones at
// once (each event's TZID plus the display zone).
namespace ics {

struct Date {
    int year;
    int month;  // 1-12
    int day;    // 1-31

    auto operator<=>(const Date&) const = default;
};

// Days since 1970-01-01 (negative before). Proleptic Gregorian.
int64_t daysFromCivil(Date date);
Date civilFromDays(int64_t days);

// 0 = Monday ... 6 = Sunday (ISO order, matching RRULE's MO..SU).
int weekdayOf(Date date);
int daysInMonth(int year, int month);

// A DATE or DATE-TIME property value, before any time zone is applied.
struct DateTimeValue {
    Date date;
    int secondOfDay = 0;  // 0 for DATE values
    bool isDate = false;  // VALUE=DATE (all-day)
    bool isUtc = false;   // trailing 'Z'
};

// Parses "YYYYMMDD", "YYYYMMDDTHHMMSS" or "YYYYMMDDTHHMMSSZ".
std::optional<DateTimeValue> parseDateTime(std::string_view text);

// Seconds since the Unix epoch for a wall-clock time at a fixed UTC offset.
int64_t toUnix(Date date, int secondOfDay, int32_t utcOffsetSeconds);

}  // namespace ics
