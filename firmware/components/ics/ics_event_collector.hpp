#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ics_datetime.hpp"

namespace ics {

struct Occurrence {
    int64_t start;  // Unix seconds
    int64_t end;
    bool allDay;
    std::string summary;  // unescaped; empty if the event has no title
};

struct CollectorOptions {
    int64_t now;               // Unix seconds; occurrences that already ended are dropped
    int32_t displayUtcOffset;  // all-day and floating times are anchored here
    size_t maxResults = 10;
    int horizonDays = 366;  // occurrences starting later than now + this are ignored
};

// Consumes unfolded ICS content lines (see LineReader) from one or more
// calendars back to back, and keeps the next `maxResults` occurrences that
// haven't ended yet -- expanding recurring events, honouring EXDATE,
// RECURRENCE-ID overrides and cancelled instances, and de-duplicating an
// event that shows up in two calendars.
//
// Memory stays bounded regardless of how much history the feeds contain:
// only the current VEVENT, the best few occurrences so far, and the keys of
// recent overrides are held.
//
// Time zones: TZIDs are resolved through the calendar's own VTIMEZONE blocks.
// Fixed-offset zones (Asia/Taipei, Asia/Tokyo, ...) are exact; zones with
// daylight saving time use their standard offset, so instances can be an
// hour off during DST. Unknown TZIDs fall back to the display offset.
class EventCollector {
public:
    explicit EventCollector(CollectorOptions options);

    void onLine(std::string_view line);

    // Sorted by start time, then summary.
    std::vector<Occurrence> takeResults();

private:
    struct TimeProp {
        DateTimeValue value;
        int32_t utcOffset;
        int64_t unix;
    };

    struct PendingEvent {
        std::optional<TimeProp> dtstart;
        std::optional<TimeProp> dtend;
        std::optional<TimeProp> recurrenceId;
        std::string rrule;
        std::vector<int64_t> exdates;
        uint64_t uidHash = 0;
        std::string summary;
        bool cancelled = false;
    };

    struct Candidate {
        Occurrence occurrence;
        uint64_t uidHash;
        int64_t originalStart;  // the instance an override replaces; == start otherwise
        bool isOverride;
    };

    void handleEventProperty(std::string_view name, std::string_view params, std::string_view value);
    void handleTimezoneProperty(std::string_view name, std::string_view value);
    void finishEvent();
    void finishTimezone();

    std::optional<TimeProp> resolveTime(std::string_view params, std::string_view value) const;
    int32_t offsetForTzid(std::string_view tzid) const;

    void addOverrideKey(uint64_t uidHash, int64_t originalStart);
    bool isOverridden(uint64_t uidHash, int64_t originalStart) const;
    // Returns false if the candidate list is full of earlier occurrences.
    bool insert(Candidate candidate);

    size_t capacity() const;

    CollectorOptions options_;
    int64_t horizonEnd_;

    // Component nesting: properties only count when they belong directly to
    // the VEVENT/VTIMEZONE being read (not, say, a VALARM's SUMMARY).
    enum class Component { none, event, timezone, timezoneRule, other };
    std::vector<Component> stack_;

    PendingEvent event_;

    std::string tzid_;
    std::optional<int32_t> tzStandardOffset_;
    std::optional<int32_t> tzAnyOffset_;
    std::vector<std::pair<std::string, int32_t>> tzOffsets_;
    bool inStandardRule_ = false;

    std::vector<std::pair<uint64_t, int64_t>> overrideKeys_;
    std::vector<Candidate> candidates_;  // sorted
};

}  // namespace ics
