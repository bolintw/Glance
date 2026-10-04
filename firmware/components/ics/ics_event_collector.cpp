#include "ics_event_collector.hpp"

#include <algorithm>

#include "ics_rrule.hpp"

namespace ics {

namespace {

// Room beyond maxResults for occurrences an override arriving later in the
// stream may still knock out (an override can come before or after the
// recurring event it modifies -- Google's order varies between downloads).
constexpr size_t kOverrideSlack = 16;

// Overrides of instances that started longer ago than this can't affect
// anything still upcoming, so their keys aren't kept.
constexpr int64_t kMaxTrackedEventSeconds = 31 * 86400;

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
               return lower(x) == lower(y);
           });
}

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

uint64_t fnv1a(std::string_view text) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : text) {
        hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ull;
    }
    return hash;
}

struct Property {
    std::string_view name;
    std::string_view params;  // ";KEY=VALUE;..." or empty
    std::string_view value;
};

// NAME;PARAM=...:VALUE -- the value starts at the first ':' outside quotes.
std::optional<Property> splitProperty(std::string_view line) {
    bool inQuotes = false;
    size_t nameEnd = std::string_view::npos;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && c == ';' && nameEnd == std::string_view::npos) {
            nameEnd = i;
        } else if (!inQuotes && c == ':') {
            if (nameEnd == std::string_view::npos) {
                nameEnd = i;
            }
            return Property{line.substr(0, nameEnd), line.substr(nameEnd, i - nameEnd), line.substr(i + 1)};
        }
    }
    return std::nullopt;
}

std::string_view paramValue(std::string_view params, std::string_view key) {
    while (!params.empty()) {
        params.remove_prefix(1);  // ';'
        size_t next = params.find(';');
        std::string_view part = params.substr(0, next);
        params = next == std::string_view::npos ? std::string_view() : params.substr(next);
        size_t eq = part.find('=');
        if (eq != std::string_view::npos && iequals(part.substr(0, eq), key)) {
            std::string_view value = part.substr(eq + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        }
    }
    return {};
}

// RFC 5545 TEXT unescaping, flattened to one line for display.
std::string unescapeText(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[++i];
            out.push_back((next == 'n' || next == 'N') ? ' ' : next);
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

// "+0800", "-0500", "+053000" -> seconds east of UTC.
std::optional<int32_t> parseUtcOffset(std::string_view text) {
    if (text.size() != 5 && text.size() != 7) {
        return std::nullopt;
    }
    int sign = text[0] == '-' ? -1 : (text[0] == '+' ? 1 : 0);
    if (sign == 0) {
        return std::nullopt;
    }
    int32_t parts[3] = {0, 0, 0};
    for (size_t p = 0; p * 2 + 1 < text.size(); p++) {
        char hi = text[1 + p * 2];
        char lo = text[2 + p * 2];
        if (hi < '0' || hi > '9' || lo < '0' || lo > '9') {
            return std::nullopt;
        }
        parts[p] = (hi - '0') * 10 + (lo - '0');
    }
    return sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
}

}  // namespace

EventCollector::EventCollector(CollectorOptions options)
    : options_(options), horizonEnd_(options.now + int64_t{options.horizonDays} * 86400) {}

void EventCollector::onLine(std::string_view line) {
    auto prop = splitProperty(line);
    if (!prop) {
        return;
    }
    Component top = stack_.empty() ? Component::none : stack_.back();

    if (iequals(prop->name, "BEGIN")) {
        Component next = Component::other;
        if (iequals(prop->value, "VCALENDAR")) {
            next = Component::none;
        } else if (top == Component::none && iequals(prop->value, "VEVENT")) {
            next = Component::event;
            event_ = PendingEvent{};
        } else if (top == Component::none && iequals(prop->value, "VTIMEZONE")) {
            next = Component::timezone;
            tzid_.clear();
            tzStandardOffset_.reset();
            tzAnyOffset_.reset();
        } else if (top == Component::timezone &&
                   (iequals(prop->value, "STANDARD") || iequals(prop->value, "DAYLIGHT"))) {
            next = Component::timezoneRule;
            inStandardRule_ = iequals(prop->value, "STANDARD");
        }
        stack_.push_back(next);
        return;
    }
    if (iequals(prop->name, "END")) {
        if (stack_.empty()) {
            return;
        }
        stack_.pop_back();
        if (top == Component::event) {
            finishEvent();
        } else if (top == Component::timezone) {
            finishTimezone();
        }
        return;
    }

    if (top == Component::event) {
        handleEventProperty(prop->name, prop->params, prop->value);
    } else if (top == Component::timezone || top == Component::timezoneRule) {
        handleTimezoneProperty(prop->name, prop->value);
    }
}

void EventCollector::handleEventProperty(std::string_view name, std::string_view params, std::string_view value) {
    if (iequals(name, "DTSTART")) {
        event_.dtstart = resolveTime(params, value);
    } else if (iequals(name, "DTEND")) {
        event_.dtend = resolveTime(params, value);
    } else if (iequals(name, "RECURRENCE-ID")) {
        event_.recurrenceId = resolveTime(params, value);
    } else if (iequals(name, "RRULE")) {
        event_.rrule.assign(value);
    } else if (iequals(name, "EXDATE")) {
        // Google writes one EXDATE per line; others comma-separate a list.
        while (!value.empty()) {
            size_t comma = value.find(',');
            if (auto t = resolveTime(params, value.substr(0, comma))) {
                event_.exdates.push_back(t->unix);
            }
            value = comma == std::string_view::npos ? std::string_view() : value.substr(comma + 1);
        }
    } else if (iequals(name, "UID")) {
        event_.uidHash = fnv1a(value);
    } else if (iequals(name, "SUMMARY")) {
        event_.summary = unescapeText(value);
    } else if (iequals(name, "STATUS")) {
        event_.cancelled = iequals(value, "CANCELLED");
    }
}

void EventCollector::handleTimezoneProperty(std::string_view name, std::string_view value) {
    if (iequals(name, "TZID")) {
        tzid_.assign(value);
    } else if (iequals(name, "TZOFFSETTO")) {
        if (auto offset = parseUtcOffset(value)) {
            tzAnyOffset_ = *offset;
            if (inStandardRule_) {
                tzStandardOffset_ = *offset;
            }
        }
    }
}

void EventCollector::finishTimezone() {
    auto offset = tzStandardOffset_ ? tzStandardOffset_ : tzAnyOffset_;
    if (tzid_.empty() || !offset) {
        return;
    }
    for (auto& [name, existing] : tzOffsets_) {
        if (name == tzid_) {
            existing = *offset;
            return;
        }
    }
    tzOffsets_.emplace_back(tzid_, *offset);
}

int32_t EventCollector::offsetForTzid(std::string_view tzid) const {
    for (const auto& [name, offset] : tzOffsets_) {
        if (name == tzid) {
            return offset;
        }
    }
    return options_.displayUtcOffset;
}

std::optional<EventCollector::TimeProp> EventCollector::resolveTime(std::string_view params,
                                                                    std::string_view value) const {
    auto parsed = parseDateTime(value);
    if (!parsed) {
        return std::nullopt;
    }
    if (iequals(paramValue(params, "VALUE"), "DATE")) {
        parsed->isDate = true;
        parsed->secondOfDay = 0;
    }
    int32_t offset = options_.displayUtcOffset;  // all-day and floating times
    if (parsed->isUtc) {
        offset = 0;
    } else if (!parsed->isDate) {
        std::string_view tzid = paramValue(params, "TZID");
        if (!tzid.empty()) {
            offset = offsetForTzid(tzid);
        }
    }
    return TimeProp{*parsed, offset, toUnix(parsed->date, parsed->secondOfDay, offset)};
}

void EventCollector::finishEvent() {
    if (!event_.dtstart) {
        return;
    }
    const TimeProp& start = *event_.dtstart;
    const bool allDay = start.value.isDate;
    const int64_t duration =
        event_.dtend ? std::max<int64_t>(0, event_.dtend->unix - start.unix) : (allDay ? 86400 : 0);
    const uint64_t uid = event_.uidHash;

    auto makeCandidate = [&](int64_t s, int64_t original, bool isOverride) {
        return Candidate{Occurrence{s, s + duration, allDay, event_.summary}, uid, original, isOverride};
    };
    auto upcoming = [&](int64_t s) { return s + duration > options_.now && s <= horizonEnd_; };

    if (event_.recurrenceId) {
        addOverrideKey(uid, event_.recurrenceId->unix);
        if (!event_.cancelled && upcoming(start.unix)) {
            insert(makeCandidate(start.unix, event_.recurrenceId->unix, true));
        }
        return;
    }
    if (event_.cancelled) {
        return;
    }

    auto rule = event_.rrule.empty() ? std::nullopt : parseRRule(event_.rrule);
    if (!rule) {
        // One-off, or a recurrence outside the supported subset: show its first instance.
        if (upcoming(start.unix) && !isOverridden(uid, start.unix)) {
            insert(makeCandidate(start.unix, start.unix, false));
        }
        return;
    }

    ExpandParams params{
        .start = start.value.date,
        .secondOfDay = start.value.secondOfDay,
        .utcOffset = start.utcOffset,
        // An instance that began up to `duration` (plus a day of slack) before now may still be running.
        .fastForwardToDay = floorDiv(options_.now - duration + start.utcOffset, 86400) - 1,
    };

    size_t produced = 0;
    expandRRule(*rule, params, [&](Date date) {
        int64_t s = toUnix(date, start.value.secondOfDay, start.utcOffset);
        if (s > horizonEnd_) {
            return false;
        }
        if (s + duration <= options_.now || isOverridden(uid, s) ||
            std::find(event_.exdates.begin(), event_.exdates.end(), s) != event_.exdates.end()) {
            return true;
        }
        // Instances arrive in order, so once one doesn't fit, none after it will.
        return insert(makeCandidate(s, s, false)) && ++produced < capacity();
    });
}

void EventCollector::addOverrideKey(uint64_t uidHash, int64_t originalStart) {
    if (originalStart + kMaxTrackedEventSeconds < options_.now) {
        return;
    }
    overrideKeys_.emplace_back(uidHash, originalStart);
    // The recurring event may already have contributed the instance being replaced.
    std::erase_if(candidates_, [&](const Candidate& c) {
        return !c.isOverride && c.uidHash == uidHash && c.originalStart == originalStart;
    });
}

bool EventCollector::isOverridden(uint64_t uidHash, int64_t originalStart) const {
    return std::find(overrideKeys_.begin(), overrideKeys_.end(), std::make_pair(uidHash, originalStart)) !=
           overrideKeys_.end();
}

size_t EventCollector::capacity() const { return options_.maxResults + kOverrideSlack; }

bool EventCollector::insert(Candidate candidate) {
    for (const Candidate& c : candidates_) {
        if (candidate.uidHash != 0 && c.uidHash == candidate.uidHash &&
            c.occurrence.start == candidate.occurrence.start) {
            return true;  // same event via another calendar
        }
    }
    auto earlier = [](const Candidate& a, const Candidate& b) {
        if (a.occurrence.start != b.occurrence.start) {
            return a.occurrence.start < b.occurrence.start;
        }
        return a.occurrence.summary < b.occurrence.summary;
    };
    auto pos = std::upper_bound(candidates_.begin(), candidates_.end(), candidate, earlier);
    if (candidates_.size() >= capacity() && pos == candidates_.end()) {
        return false;
    }
    candidates_.insert(pos, std::move(candidate));
    if (candidates_.size() > capacity()) {
        candidates_.pop_back();
    }
    return true;
}

std::vector<Occurrence> EventCollector::takeResults() {
    std::vector<Occurrence> results;
    size_t n = std::min(candidates_.size(), options_.maxResults);
    results.reserve(n);
    for (size_t i = 0; i < n; i++) {
        results.push_back(std::move(candidates_[i].occurrence));
    }
    candidates_.clear();
    return results;
}

}  // namespace ics
