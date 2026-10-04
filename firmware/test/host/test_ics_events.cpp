// Host-side tests for ICS date math, RRULE expansion and EventCollector.
// Build and run: `make -C firmware/test/host`.
//
// The fixture-driven tests compare against fixtures/expected/*.txt, generated
// by tools/ics_oracle.py from an independent RFC 5545 implementation.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "ics_datetime.hpp"
#include "ics_event_collector.hpp"
#include "ics_line_reader.hpp"
#include "ics_rrule.hpp"

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

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
    }
    return lines;
}

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

// Same format as tools/ics_oracle.py.
std::string format(const ics::Occurrence& o) {
    int64_t local = o.start + kTaipei;
    int64_t days = floorDiv(local, 86400);
    int sod = static_cast<int>(local - days * 86400);
    ics::Date d = ics::civilFromDays(days);
    char buf[32];
    if (o.allDay) {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d all-day", d.year, d.month, d.day);
    } else {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", d.year, d.month, d.day, sod / 3600, sod / 60 % 60);
    }
    return std::string(buf) + " | " + o.summary;
}

int64_t taipeiTime(int year, int month, int day, int hour, int minute) {
    return ics::toUnix({year, month, day}, hour * 3600 + minute * 60, kTaipei);
}

std::vector<std::string> collect(const std::string& icsText, int64_t now, size_t maxResults, int horizonDays = 120) {
    ics::EventCollector collector({.now = now, .displayUtcOffset = kTaipei, .maxResults = maxResults,
                                   .horizonDays = horizonDays});
    ics::LineReader reader([&](std::string_view line) { collector.onLine(line); });
    reader.feed(icsText);
    reader.finish();
    std::vector<std::string> out;
    for (const auto& o : collector.takeResults()) {
        out.push_back(format(o));
    }
    return out;
}

std::vector<ics::Date> expand(const char* rule, ics::Date start, size_t limit,
                              std::optional<int64_t> fastForwardToDay = std::nullopt) {
    std::vector<ics::Date> dates;
    auto parsed = ics::parseRRule(rule);
    if (!parsed) {
        return dates;
    }
    ics::expandRRule(*parsed, {.start = start, .fastForwardToDay = fastForwardToDay}, [&](ics::Date d) {
        dates.push_back(d);
        return dates.size() < limit;
    });
    return dates;
}

void testDateMath() {
    CHECK(ics::daysFromCivil({1970, 1, 1}) == 0);
    CHECK(ics::weekdayOf({1970, 1, 1}) == 3);  // Thursday
    CHECK(ics::weekdayOf({2026, 10, 4}) == 6);  // Sunday
    CHECK(ics::daysInMonth(2028, 2) == 29);
    CHECK(ics::daysInMonth(2100, 2) == 28);
    CHECK(ics::daysInMonth(2000, 2) == 29);
    for (int64_t days = -800000; days < 800000; days += 997) {
        CHECK(ics::daysFromCivil(ics::civilFromDays(days)) == days);
    }
    auto utc = ics::parseDateTime("20261014T033000Z");
    CHECK(utc && utc->isUtc && !utc->isDate && utc->secondOfDay == 3 * 3600 + 30 * 60);
    auto date = ics::parseDateTime("20261006");
    CHECK(date && date->isDate);
    CHECK(!ics::parseDateTime("20260231"));
    CHECK(!ics::parseDateTime("2026-10-06"));
}

void testRRuleParsing() {
    CHECK(ics::parseRRule("FREQ=WEEKLY;WKST=SU;COUNT=5;BYDAY=WE"));
    CHECK(ics::parseRRule("FREQ=MONTHLY;BYDAY=3MO"));
    CHECK(ics::parseRRule("FREQ=MONTHLY;BYDAY=-1FR"));
    CHECK(!ics::parseRRule("FREQ=MONTHLY;BYDAY=MO;BYSETPOS=-1"));
    CHECK(!ics::parseRRule("FREQ=HOURLY"));
    CHECK(!ics::parseRRule("COUNT=3"));
    CHECK(!ics::parseRRule("FREQ=YEARLY;BYDAY=20MO"));
}

void testRfc5545WeekStartExample() {
    // RFC 5545 section 3.3.10: WKST changes which week each instance is in.
    auto mo = expand("FREQ=WEEKLY;INTERVAL=2;COUNT=4;BYDAY=TU,SU;WKST=MO", {1997, 8, 5}, 100);
    CHECK((mo == std::vector<ics::Date>{{1997, 8, 5}, {1997, 8, 10}, {1997, 8, 19}, {1997, 8, 24}}));
    auto su = expand("FREQ=WEEKLY;INTERVAL=2;COUNT=4;BYDAY=TU,SU;WKST=SU", {1997, 8, 5}, 100);
    CHECK((su == std::vector<ics::Date>{{1997, 8, 5}, {1997, 8, 17}, {1997, 8, 19}, {1997, 8, 31}}));
}

void testMonthlyAndYearlyEdges() {
    auto day31 = expand("FREQ=MONTHLY;BYMONTHDAY=31", {2026, 10, 31}, 4);
    CHECK((day31 == std::vector<ics::Date>{{2026, 10, 31}, {2026, 12, 31}, {2027, 1, 31}, {2027, 3, 31}}));
    auto thirdMonday = expand("FREQ=MONTHLY;BYDAY=3MO", {2026, 10, 19}, 3);
    CHECK((thirdMonday == std::vector<ics::Date>{{2026, 10, 19}, {2026, 11, 16}, {2026, 12, 21}}));
    auto lastFriday = expand("FREQ=MONTHLY;BYDAY=-1FR", {2026, 10, 30}, 3);
    CHECK((lastFriday == std::vector<ics::Date>{{2026, 10, 30}, {2026, 11, 27}, {2026, 12, 25}}));
    auto leapDay = expand("FREQ=YEARLY", {2024, 2, 29}, 3);
    CHECK((leapDay == std::vector<ics::Date>{{2024, 2, 29}, {2028, 2, 29}, {2032, 2, 29}}));
    auto untilDate = expand("FREQ=WEEKLY;UNTIL=20261117;BYDAY=WE", {2026, 10, 7}, 100);
    CHECK(untilDate.size() == 6 && untilDate.back() == (ics::Date{2026, 11, 11}));
}

void testFastForwardMatchesFullWalk() {
    struct Case {
        const char* rule;
        ics::Date start;
    };
    const Case cases[] = {
        {"FREQ=DAILY;INTERVAL=3", {2019, 3, 7}},
        {"FREQ=WEEKLY;INTERVAL=2;BYDAY=TU,FR;WKST=SU", {2018, 1, 2}},
        {"FREQ=MONTHLY;BYDAY=3MO", {2015, 6, 15}},
        {"FREQ=MONTHLY;INTERVAL=5;BYMONTHDAY=31", {2016, 1, 31}},
        {"FREQ=YEARLY;INTERVAL=2", {2001, 9, 30}},
    };
    const int64_t ffDay = ics::daysFromCivil({2026, 10, 4});
    for (const Case& c : cases) {
        std::vector<ics::Date> fromStart;
        for (const auto& d : expand(c.rule, c.start, 100000)) {
            if (ics::daysFromCivil(d) >= ffDay && fromStart.size() < 20) {
                fromStart.push_back(d);
            }
        }
        std::vector<ics::Date> fast;
        for (const auto& d : expand(c.rule, c.start, 1000, ffDay)) {
            if (ics::daysFromCivil(d) >= ffDay && fast.size() < 20) {
                fast.push_back(d);
            }
        }
        CHECK(fromStart.size() == 20);
        CHECK(fast == fromStart);
    }
}

void testGoogleFixtureMatchesOracle() {
    const std::string icsText = readFile(FIXTURE_DIR "/google_test_calendar.ics");
    struct Now {
        const char* name;  // fixtures/expected/<name>.txt; keep in sync with tools/ics_oracle.py
        int64_t unix;
    };
    const Now nows[] = {
        {"2026-10-04T1530", taipeiTime(2026, 10, 4, 15, 30)},
        {"2026-10-13T0900", taipeiTime(2026, 10, 13, 9, 0)},
        {"2026-10-20T2350", taipeiTime(2026, 10, 20, 23, 50)},
        {"2026-11-25T1200", taipeiTime(2026, 11, 25, 12, 0)},
        {"2027-03-01T0800", taipeiTime(2027, 3, 1, 8, 0)},
    };
    for (const Now& now : nows) {
        auto expected = splitLines(readFile(std::string(FIXTURE_DIR "/expected/") + now.name + ".txt"));
        CHECK(expected.size() == 200);

        auto full = collect(icsText, now.unix, 200);
        if (full != expected) {
            std::printf("  mismatch at now=%s (got %zu, expected %zu)\n", now.name, full.size(), expected.size());
            for (size_t i = 0; i < std::max(full.size(), expected.size()); i++) {
                std::string got = i < full.size() ? full[i] : "<none>";
                std::string want = i < expected.size() ? expected[i] : "<none>";
                if (got != want) {
                    std::printf("    [%zu] got  %s\n    [%zu] want %s\n", i, got.c_str(), i, want.c_str());
                    break;
                }
            }
        }
        CHECK(full == expected);

        // Small N exercises the bounded candidate list and early stopping.
        auto top10 = collect(icsText, now.unix, 10);
        CHECK(top10 == std::vector<std::string>(expected.begin(), expected.begin() + 10));
    }
}

// Minimal calendar wrapper for synthetic cases.
std::string calendar(const std::string& body) {
    return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n" + body + "END:VCALENDAR\r\n";
}

const std::string kWeeklyParent =
    "BEGIN:VEVENT\r\nUID:standup\r\nSUMMARY:Standup\r\n"
    "DTSTART;TZID=Asia/Taipei:20261005T090000\r\nDTEND;TZID=Asia/Taipei:20261005T091500\r\n"
    "RRULE:FREQ=WEEKLY;COUNT=3\r\nEND:VEVENT\r\n";
const std::string kMovedSecondInstance =
    "BEGIN:VEVENT\r\nUID:standup\r\nSUMMARY:Standup (moved)\r\n"
    "RECURRENCE-ID;TZID=Asia/Taipei:20261012T090000\r\n"
    "DTSTART;TZID=Asia/Taipei:20261013T100000\r\nDTEND;TZID=Asia/Taipei:20261013T101500\r\nEND:VEVENT\r\n";

void testOverrideOrderDoesNotMatter() {
    const int64_t now = taipeiTime(2026, 10, 1, 0, 0);
    const std::vector<std::string> expected = {
        "2026-10-05 09:00 | Standup",
        "2026-10-13 10:00 | Standup (moved)",
        "2026-10-19 09:00 | Standup",
    };
    CHECK(collect(calendar(kWeeklyParent + kMovedSecondInstance), now, 10) == expected);
    CHECK(collect(calendar(kMovedSecondInstance + kWeeklyParent), now, 10) == expected);
}

void testCancelledInstanceAndCommaExdate() {
    const int64_t now = taipeiTime(2026, 10, 1, 0, 0);
    std::string cancelled =
        "BEGIN:VEVENT\r\nUID:standup\r\nSTATUS:CANCELLED\r\n"
        "RECURRENCE-ID;TZID=Asia/Taipei:20261012T090000\r\n"
        "DTSTART;TZID=Asia/Taipei:20261012T090000\r\nEND:VEVENT\r\n";
    CHECK((collect(calendar(kWeeklyParent + cancelled), now, 10) ==
           std::vector<std::string>{"2026-10-05 09:00 | Standup", "2026-10-19 09:00 | Standup"}));

    std::string withExdates =
        "BEGIN:VEVENT\r\nUID:daily\r\nSUMMARY:Daily\r\nDTSTART:20261005T010000Z\r\nDTEND:20261005T013000Z\r\n"
        "RRULE:FREQ=DAILY;COUNT=4\r\nEXDATE:20261006T010000Z,20261007T010000Z\r\nEND:VEVENT\r\n";
    CHECK((collect(calendar(withExdates), now, 10) ==
           std::vector<std::string>{"2026-10-05 09:00 | Daily", "2026-10-08 09:00 | Daily"}));
}

void testAlarmSummaryAndDuplicatesAndFallbacks() {
    const int64_t now = taipeiTime(2026, 10, 1, 0, 0);
    std::string withAlarm =
        "BEGIN:VEVENT\r\nUID:a\r\nSUMMARY:Dentist\r\nDTSTART:20261002T010000Z\r\n"
        "BEGIN:VALARM\r\nACTION:EMAIL\r\nSUMMARY:Alarm text\r\nEND:VALARM\r\nEND:VEVENT\r\n";
    CHECK((collect(calendar(withAlarm), now, 10) == std::vector<std::string>{"2026-10-02 09:00 | Dentist"}));

    // The same event exported by two calendars shows up once.
    CHECK((collect(calendar(withAlarm) + calendar(withAlarm), now, 10) ==
           std::vector<std::string>{"2026-10-02 09:00 | Dentist"}));

    // Unsupported rule: degrade to the first instance instead of dropping it.
    std::string unsupported =
        "BEGIN:VEVENT\r\nUID:b\r\nSUMMARY:Last weekday\r\nDTSTART;VALUE=DATE:20261030\r\n"
        "RRULE:FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1\r\nEND:VEVENT\r\n";
    CHECK((collect(calendar(unsupported), now, 10) == std::vector<std::string>{"2026-10-30 all-day | Last weekday"}));

    // Unknown TZID falls back to the display zone; escapes are undone.
    std::string unknownZone =
        "BEGIN:VEVENT\r\nUID:c\r\nSUMMARY:a\\, b\\; c\\nd\r\nDTSTART;TZID=Mars/Olympus:20261003T080000\r\nEND:VEVENT\r\n";
    CHECK((collect(calendar(unknownZone), now, 10) == std::vector<std::string>{"2026-10-03 08:00 | a, b; c d"}));
}

}  // namespace

int main() {
    testDateMath();
    testRRuleParsing();
    testRfc5545WeekStartExample();
    testMonthlyAndYearlyEdges();
    testFastForwardMatchesFullWalk();
    testGoogleFixtureMatchesOracle();
    testOverrideOrderDoesNotMatter();
    testCancelledInstanceAndCommaExdate();
    testAlarmSummaryAndDuplicatesAndFallbacks();

    if (failures == 0) {
        std::printf("all ics_events tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
