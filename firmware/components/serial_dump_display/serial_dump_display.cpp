#include "serial_dump_display.hpp"

#include <array>
#include <cstdio>
#include <vector>

#include "esp_log.h"

namespace {
constexpr const char* kTag = "serial_dump_display";
constexpr std::array<uint8_t, 4> kMagic = {'G', 'L', 'N', 'C'};
constexpr uint8_t kTypeFrame = 0x01;

uint16_t checksum(std::span<const uint8_t> data) {
    uint16_t sum = 0;
    for (uint8_t b : data) {
        sum = static_cast<uint16_t>(sum + b);
    }
    return sum;
}

void appendLe32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}
}  // namespace

SerialDumpDisplay::SerialDumpDisplay(FrameSize frame) : frame_(frame) {}

void SerialDumpDisplay::init() {
    ESP_LOGI(kTag, "simulator backend ready (%zux%zu)", frame_.width, frame_.height);
}

void SerialDumpDisplay::clear() {
    std::vector<uint8_t> blank(frame_.framebufferSize(), 0xFF);
    sendFramePacket(blank);
}

void SerialDumpDisplay::flush(std::span<const uint8_t> framebuffer) { sendFramePacket(framebuffer); }

void SerialDumpDisplay::sleep() { ESP_LOGI(kTag, "simulator backend sleep (no-op)"); }

void SerialDumpDisplay::sendFramePacket(std::span<const uint8_t> framebuffer) {
    std::vector<uint8_t> packet;
    packet.reserve(kMagic.size() + 1 + 4 + framebuffer.size() + 2);

    packet.insert(packet.end(), kMagic.begin(), kMagic.end());
    packet.push_back(kTypeFrame);
    appendLe32(packet, static_cast<uint32_t>(framebuffer.size()));
    packet.insert(packet.end(), framebuffer.begin(), framebuffer.end());

    uint16_t sum = checksum(framebuffer);
    packet.push_back(static_cast<uint8_t>(sum & 0xFF));
    packet.push_back(static_cast<uint8_t>((sum >> 8) & 0xFF));

    // Raw write to stdout: shares the console UART with ESP_LOG output, by
    // design (see header) -- the PC tool tells the two apart via kMagic.
    std::fwrite(packet.data(), 1, packet.size(), stdout);
    std::fflush(stdout);
}
