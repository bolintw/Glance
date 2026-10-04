#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "ics_datetime.hpp"

namespace ics {

struct ByDay {
    int weekday;      // 0 = MO ... 6 = SU
    int ordinal = 0;  // 0 = every such weekday; +n / -n = nth from start / end of month
};

// The subset of RFC 5545 RRULE we support: FREQ=DAILY/WEEKLY/MONTHLY/YEARLY
// with INTERVAL, COUNT, UNTIL, BYDAY, BYMONTHDAY, BYMONTH and WKST. That
// covers every recurrence Google Calendar's UI can produce.
struct RRule {
    enum class Freq { daily, weekly, monthly, yearly };

    Freq freq = Freq::daily;
    int interval = 1;
    std::optional<int> count;
    std::optional<DateTimeValue> until;
    std::vector<ByDay> byDay;
    std::vector<int> byMonthDay;  // negative counts from the end of the month
    std::vector<int> byMonth;
    int weekStart = 0;  // MO
};

// nullopt if the rule is malformed or uses parts outside the supported subset
// (BYSETPOS, BYWEEKNO, BYYEARDAY, BYHOUR, sub-daily FREQ, ...). Callers
// degrade to treating the event as a one-off at its DTSTART.
std::optional<RRule> parseRRule(std::string_view text);

struct ExpandParams {
    Date start;                // DTSTART's date in the event's own zone
    int secondOfDay = 0;       // DTSTART's time of day (0 for all-day)
    int32_t utcOffset = 0;     // the event's zone, only used to compare against a UTC UNTIL
    // Lets rules without COUNT skip whole periods that end before this day
    // (days since epoch, event-local) instead of walking from DTSTART. Must
    // be conservative: dates before it may still be produced.
    std::optional<int64_t> fastForwardToDay = std::nullopt;
};

// Calls onDate with each date the rule fires on, in chronological order,
// starting with DTSTART itself (which RFC 5545 always counts as the first
// instance). Stops when onDate returns false, COUNT is used up, UNTIL is
// passed, or a safety cap on scanned periods is hit. COUNT counts every
// generated instance, including ones the caller later drops via EXDATE.
void expandRRule(const RRule& rule, const ExpandParams& params, const std::function<bool(Date)>& onDate);

}  // namespace ics
