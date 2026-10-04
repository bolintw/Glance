#include "ics_datetime.hpp"

namespace ics {

// Howard Hinnant's days_from_civil / civil_from_days
// (https://howardhinnant.github.io/date_algorithms.html).
int64_t daysFromCivil(Date date) {
    int64_t y = date.year - (date.month <= 2 ? 1 : 0);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t mp = (date.month + 9) % 12;
    int64_t doy = (153 * mp + 2) / 5 + date.day - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date civilFromDays(int64_t days) {
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    int64_t doe = days - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    int year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
    return {year, month, day};
}

int weekdayOf(Date date) {
    // 1970-01-01 was a Thursday (3 in Monday-based numbering).
    int64_t w = (daysFromCivil(date) + 3) % 7;
    return static_cast<int>(w < 0 ? w + 7 : w);
}

int daysInMonth(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return (month == 2 && leap) ? 29 : kDays[month - 1];
}

namespace {
std::optional<int> digits(std::string_view text, size_t pos, size_t count) {
    if (pos + count > text.size()) {
        return std::nullopt;
    }
    int value = 0;
    for (size_t i = pos; i < pos + count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return std::nullopt;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}
}  // namespace

std::optional<DateTimeValue> parseDateTime(std::string_view text) {
    auto year = digits(text, 0, 4);
    auto month = digits(text, 4, 2);
    auto day = digits(text, 6, 2);
    if (!year || !month || !day || *month < 1 || *month > 12 || *day < 1 || *day > daysInMonth(*year, *month)) {
        return std::nullopt;
    }
    DateTimeValue value{.date = {*year, *month, *day}};
    if (text.size() == 8) {
        value.isDate = true;
        return value;
    }
    if (text.size() < 15 || text[8] != 'T') {
        return std::nullopt;
    }
    auto hour = digits(text, 9, 2);
    auto minute = digits(text, 11, 2);
    auto second = digits(text, 13, 2);
    if (!hour || !minute || !second || *hour > 23 || *minute > 59 || *second > 60) {
        return std::nullopt;
    }
    value.secondOfDay = *hour * 3600 + *minute * 60 + *second;
    value.isUtc = text.size() == 16 && text[15] == 'Z';
    return value;
}

int64_t toUnix(Date date, int secondOfDay, int32_t utcOffsetSeconds) {
    return daysFromCivil(date) * 86400 + secondOfDay - utcOffsetSeconds;
}

}  // namespace ics
