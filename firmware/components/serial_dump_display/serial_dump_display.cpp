#include "serial_dump_display.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

namespace {
constexpr const char* kTag = "serial_dump_display";
constexpr uint8_t kTypeFrame = 0x01;
// Sending the whole ~64KB base64 blob as one single printf'd line was
// unreliable (found while debugging why frame lines kept arriving
// truncated/missing, even on the plain secondary-console path that's
// otherwise been solid all through bring-up). Splitting it into modest
// fixed-size lines avoids relying on any single write/line being able to
// carry tens of KB at once.
constexpr size_t kChunkChars = 512;

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
    // [TYPE 1B][LENGTH 4B LE][PAYLOAD][CHECKSUM 2B LE], base64-encoded and
    // sent as a series of small plain-text lines (see display_sim.py for
    // the matching reassembly logic):
    //   GLNC:<seq>:<total>:<base64 chunk>
    std::vector<uint8_t> packet;
    packet.reserve(1 + 4 + framebuffer.size() + 2);
    packet.push_back(kTypeFrame);
    appendLe32(packet, static_cast<uint32_t>(framebuffer.size()));
    packet.insert(packet.end(), framebuffer.begin(), framebuffer.end());
    uint16_t sum = checksum(framebuffer);
    packet.push_back(static_cast<uint8_t>(sum & 0xFF));
    packet.push_back(static_cast<uint8_t>((sum >> 8) & 0xFF));

    size_t encodedLen = 0;
    mbedtls_base64_encode(nullptr, 0, &encodedLen, packet.data(), packet.size());
    std::vector<unsigned char> encoded(encodedLen);
    size_t written = 0;
    int ret = mbedtls_base64_encode(encoded.data(), encoded.size(), &written, packet.data(), packet.size());
    if (ret != 0) {
        ESP_LOGE(kTag, "base64 encode failed: %d", ret);
        return;
    }

    size_t totalChunks = (written + kChunkChars - 1) / kChunkChars;
    for (size_t seq = 0; seq < totalChunks; seq++) {
        size_t offset = seq * kChunkChars;
        size_t len = std::min(kChunkChars, written - offset);
        printf("GLNC:%u:%u:%.*s\n", static_cast<unsigned>(seq), static_cast<unsigned>(totalChunks),
               static_cast<int>(len), encoded.data() + offset);
        fflush(stdout);
        // At the default 100Hz tick rate, pdMS_TO_TICKS(2) truncates to 0
        // ticks -- vTaskDelay(0) is a bare yield, not a real wait, so this
        // loop wasn't actually giving the idle task (and thus the task
        // watchdog) a real chance to run. The task watchdog firing mid-loop
        // was corrupting the current line: the watchdog's own warning text
        // was landing in the middle of our printf output (found via a
        // headless capture script that caught "...he watchdog in time:"
        // spliced into a truncated chunk). 10ms is the smallest delay
        // that's guaranteed to actually be >=1 tick here.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
