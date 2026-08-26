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

// Backend-agnostic interface for the panel. Upper layers (rendering,
// eventually LVGL) only ever talk to this -- swapping the real EPD for a
// PC-side simulator backend (M2) is just injecting a different
// implementation, nothing above this interface changes.
//
// Framebuffer format: 1 bit per pixel, packed MSB-first, row-major,
// size == width * height / 8. Bit value 1 == white, 0 == black (matches
// LVGL's LV_COLOR_FORMAT_I1 default palette).
class Display {
public:
    virtual ~Display() = default;

    virtual void init() = 0;
    virtual void clear() = 0;
    virtual void flush(std::span<const uint8_t> framebuffer) = 0;
    virtual void sleep() = 0;
};
