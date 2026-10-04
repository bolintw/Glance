// Host-side tests for Canvas: pixels, rounded rects, text and ellipsizing.
// Build and run: `make -C firmware/test/host`.

#include <cstdio>
#include <string>
#include <vector>

#include "canvas.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

constexpr FrameSize kFrame{.width = 32, .height = 16};

struct TestCanvas {
    std::vector<uint8_t> fb = std::vector<uint8_t>(kFrame.framebufferSize(), 0xFF);
    Canvas canvas{fb, kFrame};

    bool black(int x, int y) const {
        return !(fb[y * kFrame.bytesPerRow() + x / 8] & (0x80 >> (x % 8)));
    }
    int blackCount() const {
        int n = 0;
        for (int y = 0; y < static_cast<int>(kFrame.height); y++) {
            for (int x = 0; x < static_cast<int>(kFrame.width); x++) {
                n += black(x, y);
            }
        }
        return n;
    }
};

// Synthetic font, ascent 6: 'A' is a 3x3 block-with-hole sitting on the
// baseline; U+6E2C (測) is a 9-wide bar, to exercise multi-byte UTF-8 and
// rows wider than one byte.
constexpr uint8_t kBitmaps[] = {
    0b11100000, 0b10100000, 0b11100000,  // 'A' at offset 0
    0b11111111, 0b10000000,              // '測' at offset 3 (1 row, 2 bytes)
    0b11000000,                          // '.' at offset 5
};
constexpr Glyph kGlyphs[] = {
    {.codepoint = '.', .bitmapOffset = 5, .width = 2, .height = 1, .bearingX = 0, .bearingY = 1, .advance = 3},
    {.codepoint = 'A', .bitmapOffset = 0, .width = 3, .height = 3, .bearingX = 1, .bearingY = 3, .advance = 5},
    {.codepoint = 0x6E2C, .bitmapOffset = 3, .width = 9, .height = 1, .bearingX = 0, .bearingY = 2, .advance = 10},
};
const Font kFont{.glyphs = kGlyphs, .bitmaps = kBitmaps, .ascent = 6, .lineHeight = 8};

void testPixelsAndClipping() {
    TestCanvas t;
    t.canvas.setPixel(0, 0, Color::black);
    t.canvas.setPixel(31, 15, Color::black);
    t.canvas.setPixel(-1, 3, Color::black);
    t.canvas.setPixel(32, 3, Color::black);
    t.canvas.setPixel(3, 16, Color::black);
    CHECK(t.black(0, 0) && t.black(31, 15) && t.blackCount() == 2);

    t.canvas.fillRect(-5, -5, 7, 7, Color::black);  // clips to (0,0)-(1,1)
    CHECK(t.blackCount() == 5);
    t.canvas.fill(Color::black);
    CHECK(t.blackCount() == 32 * 16);
    t.canvas.setPixel(4, 4, Color::white);
    CHECK(!t.black(4, 4));
}

void testRoundedRect() {
    TestCanvas square;
    square.canvas.roundedRect(2, 2, 10, 8, 0, 1, Color::black);
    CHECK(square.blackCount() == 2 * 10 + 2 * 6);
    CHECK(square.black(2, 2) && square.black(11, 9));

    TestCanvas round;
    round.canvas.roundedRect(0, 0, 32, 16, 6, 2, Color::black);
    CHECK(!round.black(0, 0) && !round.black(31, 0) && !round.black(0, 15) && !round.black(31, 15));
    CHECK(round.black(16, 0) && round.black(16, 1) && !round.black(16, 2));  // 2px top edge
    CHECK(round.black(0, 8) && round.black(1, 8) && !round.black(2, 8));     // 2px left edge
    // Four-way symmetric, and every row/column through a corner has ink (no gaps).
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            CHECK(round.black(x, y) == round.black(31 - x, y));
            CHECK(round.black(x, y) == round.black(x, 15 - y));
        }
    }
    for (int i = 0; i < 6; i++) {
        bool rowInk = false, colInk = false;
        for (int j = 0; j < 6; j++) {
            rowInk |= round.black(j, i);
            colInk |= round.black(i, j);
        }
        CHECK(rowInk && colInk);
    }
}

void testDrawText() {
    TestCanvas t;
    int width = t.canvas.drawText(2, 1, "A", kFont, Color::black);
    CHECK(width == 5);
    // Baseline at y = 1 + 6 = 7; 'A' top = 7 - 3 = 4, left = 2 + 1 = 3.
    CHECK(t.black(3, 4) && t.black(4, 4) && t.black(5, 4));
    CHECK(t.black(3, 5) && !t.black(4, 5) && t.black(5, 5));
    CHECK(t.black(3, 6) && t.black(4, 6) && t.black(5, 6));
    CHECK(t.blackCount() == 8);

    TestCanvas cjk;
    CHECK(cjk.canvas.drawText(0, 0, "\xE6\xB8\xAC", kFont, Color::black) == 10);  // 測
    for (int x = 0; x < 9; x++) {
        CHECK(cjk.black(x, 4));  // baseline 6, bearingY 2
    }
    CHECK(cjk.blackCount() == 9);

    TestCanvas inverted;
    inverted.canvas.fill(Color::black);
    inverted.canvas.drawText(2, 1, "A", kFont, Color::white);
    CHECK(inverted.blackCount() == 32 * 16 - 8);
}

void testMissingAndMalformed() {
    TestCanvas t;
    // 'Z' isn't in the font: drawn as a hollow 3x4 box (ascent 6), advance 5.
    CHECK(t.canvas.drawText(0, 0, "Z", kFont, Color::black) == 5);
    CHECK(t.black(1, 2) && t.black(3, 5) && !t.black(2, 3));
    // A truncated sequence decodes to one U+FFFD per byte, never overruns.
    CHECK(measureText("\xE6\xB8", kFont) == 10);
    CHECK(measureText("A\xFF" "A", kFont) == 15);
}

void testEllipsize() {
    CHECK(measureText("AAA", kFont) == 15);
    CHECK(ellipsize("AAA", kFont, 15) == "AAA");
    // "..." is 9 wide, so 16px leaves room for one 'A'.
    CHECK(ellipsize("AAAA", kFont, 16) == "A...");
    // Never cuts inside a multi-byte character.
    CHECK(ellipsize("A\xE6\xB8\xAC\xE6\xB8\xAC", kFont, 23) == "A...");
    CHECK(ellipsize("A\xE6\xB8\xAC\xE6\xB8\xAC", kFont, 24) == "A\xE6\xB8\xAC...");
    CHECK(ellipsize("AAAA", kFont, 3) == "...");
    // A cut right after a space doesn't leave "A ...".
    CHECK(measureText(" ", kFont) == 5);  // missing glyph -> placeholder advance
    CHECK(ellipsize("A AAA", kFont, 19) == "A...");
}

}  // namespace

void testDrawBitmap() {
    // 10x2: row 0 is 1010101011, row 1 all black; 10 wide pads rows to 2 bytes.
    const uint8_t bits[] = {0xAA, 0xC0, 0x00, 0x00};
    TestCanvas t;
    t.canvas.fill(Color::white);
    t.canvas.drawBitmap(3, 4, {10, 2, bits});
    CHECK(!t.black(3, 4) && t.black(4, 4) && !t.black(5, 4) && t.black(10, 4));
    CHECK(!t.black(11, 4) && !t.black(12, 4));  // last two columns white
    CHECK(t.black(3, 5) && t.black(12, 5));
    CHECK(!t.black(13, 5) && !t.black(2, 5));   // nothing outside it
    CHECK(t.blackCount() == 4 + 10);

    // White pixels are drawn too, not skipped.
    TestCanvas dark;
    dark.canvas.fill(Color::black);
    dark.canvas.drawBitmap(0, 0, {10, 2, bits});
    CHECK(!dark.black(0, 0));

    // Clipped at the frame edge without touching anything else.
    TestCanvas clipped;
    clipped.canvas.fill(Color::white);
    clipped.canvas.drawBitmap(28, 15, {10, 2, bits});
    CHECK(clipped.black(29, 15) && clipped.blackCount() == 2);

    // Too few bytes for the stated size: draws nothing.
    TestCanvas shortBits;
    shortBits.canvas.fill(Color::white);
    shortBits.canvas.drawBitmap(0, 0, {10, 3, bits});
    CHECK(shortBits.blackCount() == 0);
}

void testGrayOverlay() {
    // 6x2 2bpp image: row 0 = levels 0 1 2 3 3 2, row 1 = all 1. Rows pad to 2 bytes.
    const uint8_t bits[] = {0b00011011, 0b11100000, 0b01010101, 0b01010000};
    GrayBitmap gray{6, 2, bits};
    CHECK(gray.level(0, 0) == 0 && gray.level(1, 0) == 1 && gray.level(2, 0) == 2 && gray.level(3, 0) == 3);
    CHECK(gray.level(4, 0) == 3 && gray.level(5, 0) == 2);
    CHECK(gray.level(0, 1) == 1 && gray.level(5, 1) == 1);

    TestCanvas t;
    t.canvas.fill(Color::white);
    t.canvas.setPixel(0, 0, Color::black);
    t.canvas.setPixel(9, 3, Color::black);  // under the overlay: the overlay wins
    GrayOverlay overlay{8, 3, gray};
    CHECK(overlay.composedLevel(t.fb, kFrame, 0, 0) == 0);   // framebuffer black
    CHECK(overlay.composedLevel(t.fb, kFrame, 1, 0) == 3);   // framebuffer white
    CHECK(overlay.composedLevel(t.fb, kFrame, 8, 3) == 0);
    CHECK(overlay.composedLevel(t.fb, kFrame, 9, 3) == 1);
    CHECK(overlay.composedLevel(t.fb, kFrame, 11, 3) == 3);
    CHECK(overlay.composedLevel(t.fb, kFrame, 13, 4) == 1);
    CHECK(overlay.composedLevel(t.fb, kFrame, 14, 3) == 3);  // just right of it
    CHECK(overlay.composedLevel(t.fb, kFrame, 8, 5) == 3);   // just below it

    // Too few bytes for the stated size: ignored, the framebuffer shows.
    GrayOverlay truncated{8, 3, {6, 3, bits}};
    CHECK(truncated.composedLevel(t.fb, kFrame, 9, 3) == 0);
}

int main() {
    testPixelsAndClipping();
    testRoundedRect();
    testDrawText();
    testMissingAndMalformed();
    testEllipsize();
    testDrawBitmap();
    testGrayOverlay();

    if (failures == 0) {
        std::printf("all canvas tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
