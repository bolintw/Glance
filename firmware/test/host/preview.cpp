// Renders the calendar screen on the Mac with the same code the firmware
// runs (calendar_view + Canvas + baked fonts), fed from the test calendar
// and CWA forecast fixtures, and writes it as a PBM.
//
//   make -C firmware/test/host preview                      # now = 2026-10-04T15:30
//   make -C firmware/test/host preview NOW=2026-11-25T12:00
//
// produces build/preview.png.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "calendar_view.hpp"
#include "ics_datetime.hpp"
#include "ics_event_collector.hpp"
#include "ics_line_reader.hpp"
#include "weather.hpp"

namespace {

constexpr int32_t kTaipei = 8 * 3600;

int64_t parseNow(const char* text) {
    int y, mo, d, h, mi;
    if (std::sscanf(text, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) != 5) {
        std::fprintf(stderr, "bad time %s, want YYYY-MM-DDTHH:MM\n", text);
        std::exit(2);
    }
    return ics::toUnix({y, mo, d}, h * 3600 + mi * 60, kTaipei);
}

}  // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/preview.pbm";
    const int64_t now = parseNow(argc > 2 ? argv[2] : "2026-10-04T15:30");

    std::ifstream in(FIXTURE_DIR "/google_test_calendar.ics", std::ios::binary);
    std::string ics{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    ics::EventCollector collector({.now = now, .displayUtcOffset = kTaipei, .maxResults = 10});
    ics::LineReader reader([&](std::string_view line) { collector.onLine(line); });
    reader.feed(ics);
    reader.finish();
    auto events = collector.takeResults();

    std::ifstream weatherIn(FIXTURE_DIR "/cwa_f-c0032-001_hsinchu.json", std::ios::binary);
    std::string weatherJson{std::istreambuf_iterator<char>(weatherIn), std::istreambuf_iterator<char>()};
    auto forecast = weather::parseCwa36Hour(weatherJson, now);

    constexpr FrameSize kFrame{.width = 800, .height = 480};
    std::vector<uint8_t> fb(kFrame.framebufferSize());
    Canvas canvas(fb, kFrame);
    calendar_view::render(canvas, now, kTaipei, events, forecast);

    std::FILE* f = std::fopen(out, "wb");
    if (!f) {
        std::perror(out);
        return 1;
    }
    // PBM uses 1 = black, the opposite of the framebuffer.
    std::fprintf(f, "P4\n%zu %zu\n", kFrame.width, kFrame.height);
    for (uint8_t b : fb) {
        std::fputc(static_cast<uint8_t>(~b), f);
    }
    std::fclose(f);
    return 0;
}
