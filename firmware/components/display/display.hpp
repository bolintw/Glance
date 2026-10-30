#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// Shared by every Display implementation (real panel or simulator) so a
// backend swap never needs a different way to describe the framebuffer's
// dimensions.
struct FrameSize {
    size_t width;
    size_t height;

    constexpr size_t bytesPerRow() const { return width / 8; }
    constexpr size_t framebufferSize() const { return width * height / 8; }
};

// A 2bpp grayscale image: 4 pixels per byte, MSB-first, each row padded to
// a whole byte. Levels run 0 = black, 1, 2, 3 = white.
struct GrayBitmap {
    int width;
    int height;
    std::span<const uint8_t> bits;

    static constexpr size_t rowBytes(int width) { return static_cast<size_t>(width + 3) / 4; }
    static constexpr size_t sizeFor(int width, int height) { return rowBytes(width) * static_cast<size_t>(height); }

    constexpr uint8_t level(int x, int y) const {
        uint8_t byte = bits[static_cast<size_t>(y) * rowBytes(width) + static_cast<size_t>(x) / 4];
        return static_cast<uint8_t>((byte >> (6 - 2 * (x % 4))) & 0x03);
    }
};

// A portrait screen on the landscape panel: drawn frame.height wide and
// frame.width tall, and turned so that the panel's left edge is its top --
// the device turned a quarter clockwise. (The other quarter is this plus
// the panel's own upside-down scan, EpdConfig::upsideDown.) The panel's
// pixel (px, py) shows the portrait picture's (frame.height - 1 - py, px).
constexpr FrameSize portraitOf(FrameSize panel) { return {panel.height, panel.width}; }

// A portrait framebuffer (portraitOf(panel)) turned onto a landscape one.
inline void turnToLandscape(std::span<const uint8_t> portrait, std::span<uint8_t> landscape, FrameSize panel) {
    const FrameSize tall = portraitOf(panel);
    for (size_t py = 0; py < panel.height; py++) {
        for (size_t px = 0; px < panel.width; px++) {
            const size_t lx = panel.height - 1 - py, ly = px;
            const bool white = portrait[ly * tall.bytesPerRow() + lx / 8] & (0x80 >> (lx % 8));
            uint8_t& byte = landscape[py * panel.bytesPerRow() + px / 8];
            const auto bit = static_cast<uint8_t>(0x80 >> (px % 8));
            byte = white ? static_cast<uint8_t>(byte | bit) : static_cast<uint8_t>(byte & ~bit);
        }
    }
}

// A grayscale picture laid over a 1bpp frame, for panels with a 4-level
// mode: the photo in privacy mode, everything else stays black and white.
struct GrayOverlay {
    int x;
    int y;
    GrayBitmap bitmap;
    // A part of it left to the framebuffer: a label drawn over a full-screen
    // photo. Empty by default.
    int cutoutX = 0, cutoutY = 0, cutoutWidth = 0, cutoutHeight = 0;
    // The framebuffer and this overlay are a portrait screen (portraitOf the
    // panel), all their coordinates portrait ones.
    bool portrait = false;

    // The 0-3 level of the panel's pixel (px, py) in the combined picture:
    // the overlay where it covers, the framebuffer (black 0 / white 3)
    // elsewhere. `frame` is the panel's.
    constexpr uint8_t composedLevel(std::span<const uint8_t> framebuffer, FrameSize frame, int px, int py) const {
        if (portrait) {
            const int lx = static_cast<int>(frame.height) - 1 - py;
            py = px;
            px = lx;
            frame = portraitOf(frame);
        }
        int ox = px - x;
        int oy = py - y;
        bool inCutout = px >= cutoutX && py >= cutoutY && px < cutoutX + cutoutWidth && py < cutoutY + cutoutHeight;
        if (!inCutout && ox >= 0 && oy >= 0 && ox < bitmap.width && oy < bitmap.height &&
            bitmap.bits.size() >= GrayBitmap::sizeFor(bitmap.width, bitmap.height)) {
            return bitmap.level(ox, oy);
        }
        bool white = framebuffer[static_cast<size_t>(py) * frame.bytesPerRow() + static_cast<size_t>(px) / 8] &
                     (0x80 >> (px % 8));
        return white ? 3 : 0;
    }
};

// Backend-agnostic interface for the panel. Upper layers (Canvas-based
// rendering) only ever talk to this -- swapping the real EPD for a
// PC-side simulator backend (M2) is just injecting a different
// implementation, nothing above this interface changes.
//
// Framebuffer format: 1 bit per pixel, packed MSB-first, row-major,
// size == width * height / 8. Bit value 1 == white, 0 == black (matches
// LVGL's LV_COLOR_FORMAT_I1 default palette).
//
// init/clear/flush return false if the panel didn't respond (the backend
// logs why). sleep is best effort.
class Display {
public:
    virtual ~Display() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual bool clear() = 0;
    [[nodiscard]] virtual bool flush(std::span<const uint8_t> framebuffer) = 0;
    // The framebuffer with a grayscale overlay. A backend without a 4-level
    // mode shows just the framebuffer, so draw a 1bpp stand-in underneath.
    [[nodiscard]] virtual bool flushGray(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay) {
        (void)overlay;
        return flush(framebuffer);
    }
    virtual void sleep() = 0;
};
