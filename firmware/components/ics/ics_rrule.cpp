#include "ics_rrule.hpp"

#include <algorithm>
#include <charconv>

namespace ics {

namespace {

constexpr int kMaxScannedPeriods = 5000;

std::optional<int> parseInt(std::string_view text) {
    int value = 0;
    const char* begin = text.data();
    if (!text.empty() && text.front() == '+') {
        begin++;
    }
    auto [ptr, ec] = std::from_chars(begin, text.data() + text.size(), value);
    if (ec != std::errc() || ptr != text.data() + text.size() || begin == ptr) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> parseWeekday(std::string_view text) {
    static constexpr std::string_view kNames[] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
    for (int i = 0; i < 7; i++) {
        if (text == kNames[i]) {
            return i;
        }
    }
    return std::nullopt;
}

template <typename Fn>
bool forEachListItem(std::string_view list, Fn&& fn) {
    while (!list.empty()) {
        size_t comma = list.find(',');
        if (!fn(list.substr(0, comma))) {
            return false;
        }
        list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
    }
    return true;
}

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

bool contains(const std::vector<int>& values, int value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool weekdayMatches(const std::vector<ByDay>& byDay, int weekday) {
    return std::any_of(byDay.begin(), byDay.end(), [&](const ByDay& b) { return b.weekday == weekday; });
}

// Days of `month` the rule fires on (MONTHLY, or one month of a YEARLY),
// sorted and unique.
std::vector<int> daysInPeriodMonth(const RRule& rule, int year, int month, int defaultDay) {
    int dim = daysInMonth(year, month);
    std::vector<int> days;
    if (!rule.byMonthDay.empty()) {
        for (int v : rule.byMonthDay) {
            int d = v > 0 ? v : dim + v + 1;
            // RFC 5545: a day that doesn't exist in this month (e.g. the 31st
            // in November) is skipped, not clamped.
            if (d >= 1 && d <= dim && (rule.byDay.empty() || weekdayMatches(rule.byDay, weekdayOf({year, month, d})))) {
                days.push_back(d);
            }
        }
    } else if (!rule.byDay.empty()) {
        int firstWeekday = weekdayOf({year, month, 1});
        int lastWeekday = weekdayOf({year, month, dim});
        for (const ByDay& b : rule.byDay) {
            int first = 1 + (b.weekday - firstWeekday + 7) % 7;
            if (b.ordinal == 0) {
                for (int d = first; d <= dim; d += 7) {
                    days.push_back(d);
                }
            } else if (b.ordinal > 0) {
                int d = first + 7 * (b.ordinal - 1);
                if (d <= dim) {
                    days.push_back(d);
                }
            } else {
                int last = dim - (lastWeekday - b.weekday + 7) % 7;
                int d = last - 7 * (-b.ordinal - 1);
                if (d >= 1) {
                    days.push_back(d);
                }
            }
        }
    } else if (defaultDay <= dim) {
        days.push_back(defaultDay);
    }
    std::sort(days.begin(), days.end());
    days.erase(std::unique(days.begin(), days.end()), days.end());
    return days;
}

}  // namespace

std::optional<RRule> parseRRule(std::string_view text) {
    RRule rule;
    bool sawFreq = false;
    while (!text.empty()) {
        size_t semi = text.find(';');
        std::string_view part = text.substr(0, semi);
        text = semi == std::string_view::npos ? std::string_view() : text.substr(semi + 1);
        if (part.empty()) {
            continue;
        }
        size_t eq = part.find('=');
        if (eq == std::string_view::npos) {
            return std::nullopt;
        }
        std::string_view key = part.substr(0, eq);
        std::string_view value = part.substr(eq + 1);

        if (key == "FREQ") {
            sawFreq = true;
            if (value == "DAILY") {
                rule.freq = RRule::Freq::daily;
            } else if (value == "WEEKLY") {
                rule.freq = RRule::Freq::weekly;
            } else if (value == "MONTHLY") {
                rule.freq = RRule::Freq::monthly;
            } else if (value == "YEARLY") {
                rule.freq = RRule::Freq::yearly;
            } else {
                return std::nullopt;  // HOURLY and finer: nothing a wall calendar needs
            }
        } else if (key == "INTERVAL") {
            auto v = parseInt(value);
            if (!v || *v < 1) {
                return std::nullopt;
            }
            rule.interval = *v;
        } else if (key == "COUNT") {
            auto v = parseInt(value);
            if (!v || *v < 1) {
                return std::nullopt;
            }
            rule.count = *v;
        } else if (key == "UNTIL") {
            rule.until = parseDateTime(value);
            if (!rule.until) {
                return std::nullopt;
            }
        } else if (key == "WKST") {
            auto v = parseWeekday(value);
            if (!v) {
                return std::nullopt;
            }
            rule.weekStart = *v;
        } else if (key == "BYDAY") {
            bool ok = forEachListItem(value, [&](std::string_view item) {
                if (item.size() < 2) {
                    return false;
                }
                auto weekday = parseWeekday(item.substr(item.size() - 2));
                std::string_view ordinalText = item.substr(0, item.size() - 2);
                auto ordinal = ordinalText.empty() ? std::optional<int>(0) : parseInt(ordinalText);
                if (!weekday || !ordinal || *ordinal < -5 || *ordinal > 5) {
                    return false;
                }
                rule.byDay.push_back({*weekday, *ordinal});
                return true;
            });
            if (!ok) {
                return std::nullopt;
            }
        } else if (key == "BYMONTHDAY") {
            bool ok = forEachListItem(value, [&](std::string_view item) {
                auto v = parseInt(item);
                if (!v || *v == 0 || *v < -31 || *v > 31) {
                    return false;
                }
                rule.byMonthDay.push_back(*v);
                return true;
            });
            if (!ok) {
                return std::nullopt;
            }
        } else if (key == "BYMONTH") {
            bool ok = forEachListItem(value, [&](std::string_view item) {
                auto v = parseInt(item);
                if (!v || *v < 1 || *v > 12) {
                    return false;
                }
                rule.byMonth.push_back(*v);
                return true;
            });
            if (!ok) {
                return std::nullopt;
            }
        } else if (!key.starts_with("X-")) {
            return std::nullopt;  // BYSETPOS, BYWEEKNO, BYYEARDAY, BYHOUR, ...
        }
    }
    if (!sawFreq) {
        return std::nullopt;
    }
    // A YEARLY rule with an ordinal BYDAY but no BYMONTH means "the nth
    // weekday of the year" -- valid RFC 5545, but not something Google
    // generates, so leave it out rather than implement it.
    if (rule.freq == RRule::Freq::yearly && rule.byMonth.empty() &&
        std::any_of(rule.byDay.begin(), rule.byDay.end(), [](const ByDay& b) { return b.ordinal != 0; })) {
        return std::nullopt;
    }
    std::sort(rule.byMonth.begin(), rule.byMonth.end());
    return rule;
}

void expandRRule(const RRule& rule, const ExpandParams& params, const std::function<bool(Date)>& onDate) {
    const int64_t startDay = daysFromCivil(params.start);
    int remaining = rule.count.value_or(-1);
    // Fast-forwarding would skip instances COUNT has to see.
    std::optional<int64_t> fastForward = rule.count ? std::nullopt : params.fastForwardToDay;

    auto pastUntil = [&](Date date) {
        if (!rule.until) {
            return false;
        }
        const DateTimeValue& until = *rule.until;
        if (until.isDate) {
            return date > until.date;
        }
        if (until.isUtc) {
            return toUnix(date, params.secondOfDay, params.utcOffset) > toUnix(until.date, until.secondOfDay, 0);
        }
        return date > until.date || (date == until.date && params.secondOfDay > until.secondOfDay);
    };

    // Returns false once expansion should stop.
    auto emit = [&](Date date) {
        if (pastUntil(date)) {
            return false;
        }
        if (!onDate(date)) {
            return false;
        }
        return remaining < 0 || --remaining > 0;
    };

    // DTSTART is always the first instance, whether or not the rule matches it.
    bool skipStart = fastForward && startDay < *fastForward;
    if (!skipStart && !emit(params.start)) {
        return;
    }

    // Visits candidate dates period by period; only dates after DTSTART count
    // (DTSTART itself was emitted above).
    auto visit = [&](Date date) {
        if (daysFromCivil(date) <= startDay) {
            return true;
        }
        if (!rule.byMonth.empty() && rule.freq != RRule::Freq::yearly && !contains(rule.byMonth, date.month)) {
            return true;
        }
        return emit(date);
    };

    auto firstPeriod = [&](int64_t periodLength, int64_t periodZero) -> int64_t {
        if (!fastForward) {
            return 0;
        }
        return std::max<int64_t>(0, floorDiv(*fastForward - periodZero, periodLength * rule.interval) - 1);
    };

    switch (rule.freq) {
        case RRule::Freq::daily: {
            int64_t k0 = firstPeriod(1, startDay);
            for (int64_t k = k0; k < k0 + kMaxScannedPeriods; k++) {
                Date date = civilFromDays(startDay + k * rule.interval);
                if (!rule.byDay.empty() && !weekdayMatches(rule.byDay, weekdayOf(date))) {
                    continue;
                }
                if (!rule.byMonthDay.empty()) {
                    auto days = daysInPeriodMonth(rule, date.year, date.month, date.day);
                    if (std::find(days.begin(), days.end(), date.day) == days.end()) {
                        continue;
                    }
                }
                if (!visit(date)) {
                    return;
                }
            }
            break;
        }
        case RRule::Freq::weekly: {
            int64_t weekZero = startDay - (weekdayOf(params.start) - rule.weekStart + 7) % 7;
            int64_t k0 = firstPeriod(7, weekZero);
            for (int64_t k = k0; k < k0 + kMaxScannedPeriods; k++) {
                int64_t weekBegin = weekZero + 7 * rule.interval * k;
                for (int d = 0; d < 7; d++) {
                    int weekday = (rule.weekStart + d) % 7;
                    bool fires = rule.byDay.empty() ? weekday == weekdayOf(params.start)
                                                    : weekdayMatches(rule.byDay, weekday);
                    if (fires && !visit(civilFromDays(weekBegin + d))) {
                        return;
                    }
                }
            }
            break;
        }
        case RRule::Freq::monthly: {
            int64_t monthZero = int64_t{params.start.year} * 12 + (params.start.month - 1);
            int64_t k0 = 0;
            if (fastForward) {
                Date ff = civilFromDays(*fastForward);
                k0 = std::max<int64_t>(0, floorDiv(int64_t{ff.year} * 12 + (ff.month - 1) - monthZero, rule.interval) - 1);
            }
            for (int64_t k = k0; k < k0 + kMaxScannedPeriods; k++) {
                int64_t monthIndex = monthZero + k * rule.interval;
                int year = static_cast<int>(floorDiv(monthIndex, 12));
                int month = static_cast<int>(monthIndex - int64_t{year} * 12) + 1;
                for (int d : daysInPeriodMonth(rule, year, month, params.start.day)) {
                    if (!visit({year, month, d})) {
                        return;
                    }
                }
            }
            break;
        }
        case RRule::Freq::yearly: {
            std::vector<int> months = rule.byMonth;
            if (months.empty()) {
                if (rule.byDay.empty()) {
                    months.push_back(params.start.month);
                } else {
                    for (int m = 1; m <= 12; m++) {
                        months.push_back(m);  // e.g. BYDAY=MO: every Monday of the year
                    }
                }
            }
            int64_t k0 = 0;
            if (fastForward) {
                k0 = std::max<int64_t>(0, floorDiv(civilFromDays(*fastForward).year - params.start.year, rule.interval) - 1);
            }
            for (int64_t k = k0; k < k0 + kMaxScannedPeriods; k++) {
                int year = static_cast<int>(params.start.year + k * rule.interval);
                for (int month : months) {
                    for (int d : daysInPeriodMonth(rule, year, month, params.start.day)) {
                        if (!visit({year, month, d})) {
                            return;
                        }
                    }
                }
            }
            break;
        }
    }
}

}  // namespace ics
