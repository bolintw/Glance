// Renders a sample frame with the real Canvas + baked fonts and writes it as
// a PBM, so text rendering can be checked on the Mac without flashing.
// `make -C firmware/test/host preview` turns it into build/preview.png.

#include <cstdio>
#include <vector>

#include "canvas.hpp"
#include "fonts.hpp"

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/preview.pbm";
    constexpr FrameSize kFrame{.width = 800, .height = 480};
    std::vector<uint8_t> fb(kFrame.framebufferSize());
    Canvas canvas(fb, kFrame);
    canvas.fill(Color::white);

    canvas.drawText(20, 10, "2026", kNotoSansTcBold40, Color::black);
    canvas.drawText(160, 40, "Oct.", kNotoSansTcMedium50, Color::black);
    canvas.drawText(290, 0, "14", kNotoSansTcRegular100, Color::black);
    canvas.drawText(430, 40, "三", kNotoSansTcBold40, Color::black);

    canvas.roundedRect(20, 160, 760, 300, 16, 2, Color::black);
    const char* samples[] = {
        "10/14 : 測試：非常長的標題，以及各種標點符號。的測試;,./?\"'`是否一切顯示正常呢",
        "10/15 : 測試 每天重複 Daily standup",
        "10/19 : 測試 每月重複（第三個星期一）",
        "10/21 : 龜鬱鑿齉 — 罕用字與 ˍ‾∼≒ 缺字",
        "abcdefghijklmnopqrstuvwxyz 0123456789",
    };
    int y = 175;
    for (const char* s : samples) {
        canvas.drawText(40, y, ellipsize(s, kNotoSansTcBold30, 720), kNotoSansTcBold30, Color::black);
        y += 52;
    }

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
