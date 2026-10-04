#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "display.hpp"
#include "font.hpp"

// Pure C++ (no ESP-IDF headers) so it builds and is tested on the host too,
// see firmware/test/host/.

enum class Color : uint8_t { black, white };

// A 1bpp image in the framebuffer's format: MSB-first, each row padded to a
// whole byte, 1 = white.
struct Bitmap {
    int width;
    int height;
    std::span<const uint8_t> bits;

    static constexpr size_t rowBytes(int width) { return static_cast<size_t>(width + 7) / 8; }
    static constexpr size_t sizeFor(int width, int height) { return rowBytes(width) * static_cast<size_t>(height); }
};

// Draws into a Display-format framebuffer (1bpp, MSB-first, 1 = white).
// Everything clips to the frame, so callers can draw partly off-screen.
class Canvas {
public:
    Canvas(std::span<uint8_t> framebuffer, FrameSize frame);

    void fill(Color color);
    void setPixel(int x, int y, Color color);
    void fillRect(int x, int y, int width, int height, Color color);
    // Outline only; `thickness` grows inward.
    void roundedRect(int x, int y, int width, int height, int radius, int thickness, Color color);

    // Draws UTF-8 text with its line box's top-left corner at (x, y).
    // Characters the font doesn't cover are drawn as a hollow box so a
    // missing glyph is visible instead of silently vanishing. Returns the
    // width drawn.
    int drawText(int x, int y, std::string_view utf8, const Font& font, Color color);

    // Copies the bitmap (both colors) with its top-left corner at (x, y).
    // Draws nothing if `bits` is shorter than the dimensions need.
    void drawBitmap(int x, int y, const Bitmap& bitmap);

    const FrameSize& frame() const { return frame_; }

private:
    std::span<uint8_t> framebuffer_;
    FrameSize frame_;
};

// Width drawText would use for this text.
int measureText(std::string_view utf8, const Font& font);

// `utf8` unchanged if it fits in maxWidth, otherwise its longest prefix (cut
// on a character boundary) that still fits once "..." is appended.
std::string ellipsize(std::string_view utf8, const Font& font, int maxWidth);
