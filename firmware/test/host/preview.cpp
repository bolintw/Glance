// Renders the calendar screen on the Mac with the same code the firmware
// runs (calendar_view + Canvas + baked fonts), fed from the test calendar
// and CWA forecast fixtures, and writes it as a PGM (8-bit gray, so the
// 4-gray privacy screen can be previewed too).
//
//   make -C firmware/test/host preview                      # now = 2026-10-04T15:30
//   make -C firmware/test/host preview NOW=2026-11-25T12:00
//   make -C firmware/test/host preview SCREEN=setup         # or lan, notice, joined, intruder, privacy[1], gray[1]
//
// produces build/preview.png.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

#include "calendar_view.hpp"
#include "ics_datetime.hpp"
#include "ics_event_collector.hpp"
#include "ics_line_reader.hpp"
#include "photos.hpp"
#include "setup_page.hpp"
#include "setup_view.hpp"
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
    const char* out = argc > 1 ? argv[1] : "build/preview.pgm";
    const int64_t now = parseNow(argc > 2 ? argv[2] : "2026-10-04T15:30");
    const std::string_view screen = argc > 3 ? argv[3] : "calendar";

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
    std::optional<GrayOverlay> overlay;
    if (screen == "setup") {
        setup_view::render(canvas, setup_page::wifiQrPayload("Glance-AB12", "k7m2qx9p"), "Glance-AB12", "k7m2qx9p",
                           "http://192.168.4.1");
    } else if (screen.starts_with("privacy") || screen.starts_with("gray")) {
        size_t index = screen.ends_with("1") ? 1 : 0;
        calendar_view::renderPrivate(canvas, now, kTaipei, forecast, photos::kBuiltIn[index]);
        if (screen.starts_with("gray")) {
            overlay = calendar_view::photoOverlay(photos::kBuiltInGray[index]);
        }
    } else if (screen == "lan") {
        setup_view::renderLan(canvas, "http://192.168.0.249/?t=8f3a1c0d9e2b4f6a8c1d3e5f7a9b0c2d");
    } else if (screen == "joined") {
        const std::string_view lines[] = {"請在手機上完成設定", "沒有跳出來的話請開啟 http://192.168.4.1"};
        setup_view::renderNotice(canvas, "手機已連上", lines);
    } else if (screen == "intruder") {
        const std::string_view lines[] = {"已關閉熱點", "請長按按鈕重新設定"};
        setup_view::renderNotice(canvas, "偵測到第二台裝置連線", lines);
    } else if (screen == "notice") {
        const std::string_view lines[] = {"每小時會自動重試", "長按按鈕可以重新設定"};
        setup_view::renderNotice(canvas, "WiFi 連線失敗", lines);
    } else {
        calendar_view::render(canvas, now, kTaipei, events, forecast);
    }

    std::FILE* f = std::fopen(out, "wb");
    if (!f) {
        std::perror(out);
        return 1;
    }
    // What the panel shows: the overlay's 4 levels where it covers (0 black
    // .. 3 white, as 0/85/170/255), the framebuffer elsewhere.
    GrayOverlay none{0, 0, {0, 0, {}}};
    const GrayOverlay& shown = overlay ? *overlay : none;
    std::fprintf(f, "P5\n%zu %zu\n255\n", kFrame.width, kFrame.height);
    for (int y = 0; y < static_cast<int>(kFrame.height); y++) {
        for (int x = 0; x < static_cast<int>(kFrame.width); x++) {
            std::fputc(shown.composedLevel(fb, kFrame, x, y) * 85, f);
        }
    }
    std::fclose(f);
    return 0;
}
