#pragma once

#include "display.hpp"

// M2 dev backend: packs the framebuffer into a small length-prefixed binary
// blob, base64-encodes it, and prints it as a series of small plain-text
// lines ("GLNC:<seq>:<total>:<chunk>") via the normal log output, so a
// PC-side viewer (firmware/tools/display_sim/) can render it without a
// physical EPD attached. Selected via menuconfig -> Glance Display Backend.
// Protocol (before base64/chunking) must be kept in sync with
// display_sim.py:
//   [TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE]
// CHECKSUM is a 16-bit wraparound sum of the payload bytes.
class SerialDumpDisplay : public Display {
public:
    explicit SerialDumpDisplay(FrameSize frame);

    void init() override;
    void clear() override;
    void flush(std::span<const uint8_t> framebuffer) override;
    void sleep() override;

private:
    void sendFramePacket(std::span<const uint8_t> framebuffer);

    FrameSize frame_;
};
