#pragma once

#include <functional>

#include "display.hpp"

// M2 dev backend: packs the framebuffer into a small length-prefixed binary
// blob, base64-encodes it, and prints it as a series of small plain-text
// lines ("GLNC:<seq>:<total>:<chunk>") via the normal log output, so a
// PC-side viewer (firmware/tools/display_sim/) can render it without a
// physical EPD attached. Selected via menuconfig -> Glance Display Backend.
// Protocol (before base64/chunking) must be kept in sync with
// display_sim.py:
//   [TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE]
// CHECKSUM is a 16-bit wraparound sum of the payload bytes. TYPE 0x01 is a
// 1bpp frame (the framebuffer as is), 0x02 a 2bpp one (flushGray: 4 pixels
// per byte, MSB first, 0 = black ... 3 = white).
//
// The packet is encoded and printed one line at a time, never built whole:
// with WiFi holding its buffers there's no contiguous ~64KB left on the heap
// for a full base64 copy of a frame.
class SerialDumpDisplay : public Display {
public:
    explicit SerialDumpDisplay(FrameSize frame);

    bool init() override;
    bool clear() override;
    bool flush(std::span<const uint8_t> framebuffer) override;
    bool flushGray(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay) override;
    void sleep() override;

private:
    // An empty span sends an all-white frame without allocating one.
    bool sendFramePacket(std::span<const uint8_t> framebuffer);
    // Streams one packet, asking for each payload byte as it goes.
    bool sendPacket(uint8_t type, size_t payloadSize, const std::function<uint8_t(size_t)>& payloadByte);

    FrameSize frame_;
};
