// Host-side tests for reading CWA's 36-hour forecast.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "ics_datetime.hpp"
#include "weather.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

using weather::Condition;

int64_t taipei(int y, int m, int d, int hh, int mm) { return ics::toUnix({y, m, d}, hh * 3600 + mm * 60, 8 * 3600); }

std::string readFixture(const char* name) {
    std::ifstream in(std::string(FIXTURE_DIR "/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void testConditionOf() {
    CHECK(weather::conditionOf("晴天") == Condition::sunny);
    CHECK(weather::conditionOf("晴時多雲") == Condition::partlyCloudy);
    CHECK(weather::conditionOf("多雲時晴") == Condition::partlyCloudy);
    CHECK(weather::conditionOf("多雲") == Condition::cloudy);
    CHECK(weather::conditionOf("陰天") == Condition::cloudy);
    CHECK(weather::conditionOf("陰時多雲") == Condition::cloudy);
    CHECK(weather::conditionOf("陰時多雲短暫陣雨") == Condition::rainy);
    CHECK(weather::conditionOf("多雲午後短暫雷陣雨") == Condition::rainy);
    CHECK(weather::conditionOf("有霧") == Condition::cloudy);
    CHECK(weather::conditionOf("強風") == Condition::windy);
    CHECK(weather::conditionOf("") == Condition::partlyCloudy);
}

// The fixture was fetched 2026-10-04 20:19; its periods are
// 10/04 18:00-10/05 06:00, 10/05 06:00-18:00, 10/05 18:00-10/06 06:00.
void testFixturePeriods() {
    std::string json = readFixture("cwa_f-c0032-001_hsinchu.json");

    auto tonight = weather::parseCwa36Hour(json, taipei(2026, 10, 4, 20, 19));
    CHECK(tonight && tonight->description == "晴時多雲" && tonight->condition == Condition::partlyCloudy);
    CHECK(tonight && tonight->rainChance == 20);

    // The daily 07:00 refresh reads the daytime period.
    auto morning = weather::parseCwa36Hour(json, taipei(2026, 10, 5, 7, 0));
    CHECK(morning && morning->description == "陰時多雲短暫陣雨" && morning->condition == Condition::rainy);
    CHECK(morning && morning->rainChance == 50);
    CHECK(morning && morning->minTemp <= morning->maxTemp);

    // Period boundaries: start inclusive, end exclusive.
    CHECK(weather::parseCwa36Hour(json, taipei(2026, 10, 5, 6, 0))->rainChance == 50);
    CHECK(weather::parseCwa36Hour(json, taipei(2026, 10, 5, 5, 59))->rainChance == 20);

    // Before every period: fall back to the first.
    CHECK(weather::parseCwa36Hour(json, taipei(2026, 10, 1, 0, 0))->description == "晴時多雲");
    // After every period: also the first (stale data beats none).
    CHECK(weather::parseCwa36Hour(json, taipei(2026, 10, 9, 0, 0))->description == "晴時多雲");
}

void testMinimalDocument() {
    const char* doc = R"({"records":{"location":[{"weatherElement":[
        {"elementName":"Wx","time":[{"startTime":"2026-01-01 06:00:00","endTime":"2026-01-01 18:00:00",
          "parameter":{"parameterName":"晴天","parameterValue":"1"}}]},
        {"elementName":"PoP","time":[{"startTime":"2026-01-01 06:00:00","endTime":"2026-01-01 18:00:00",
          "parameter":{"parameterName":"0"}}]},
        {"elementName":"MinT","time":[{"startTime":"2026-01-01 06:00:00","endTime":"2026-01-01 18:00:00",
          "parameter":{"parameterName":"-3"}}]},
        {"elementName":"MaxT","time":[{"startTime":"2026-01-01 06:00:00","endTime":"2026-01-01 18:00:00",
          "parameter":{"parameterName":"8"}}]}]}]}})";
    auto f = weather::parseCwa36Hour(doc, taipei(2026, 1, 1, 7, 0));
    CHECK(f && f->condition == Condition::sunny && f->rainChance == 0 && f->minTemp == -3 && f->maxTemp == 8);
}

void testRejects() {
    CHECK(!weather::parseCwa36Hour("", 0));
    CHECK(!weather::parseCwa36Hour("{\"success\":\"false\"}", 0));
    CHECK(!weather::parseCwa36Hour(R"({"records":{"location":[]}})", 0));
    // PoP not a number.
    const char* doc = R"({"records":{"location":[{"weatherElement":[
        {"elementName":"Wx","time":[{"parameter":{"parameterName":"晴天"}}]},
        {"elementName":"PoP","time":[{"parameter":{"parameterName":"n/a"}}]},
        {"elementName":"MinT","time":[{"parameter":{"parameterName":"20"}}]},
        {"elementName":"MaxT","time":[{"parameter":{"parameterName":"25"}}]}]}]}})";
    CHECK(!weather::parseCwa36Hour(doc, 0));
}

}  // namespace

int main() {
    testConditionOf();
    testFixturePeriods();
    testMinimalDocument();
    testRejects();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("all weather tests passed\n");
    return 0;
}
