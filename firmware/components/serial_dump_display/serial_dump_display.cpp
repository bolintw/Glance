#include "serial_dump_display.hpp"

#include <algorithm>
#include <cstdio>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

namespace {
constexpr const char* kTag = "serial_dump_display";
constexpr uint8_t kTypeFrame = 0x01;
constexpr uint8_t kTypeGrayFrame = 0x02;
constexpr size_t kHeaderBytes = 1 + 4;
constexpr size_t kChecksumBytes = 2;
// Each line carries 384 packet bytes = exactly 512 base64 characters. Since
// 384 is a multiple of 3, no line but the last needs padding, so the viewer
// can simply concatenate the lines and decode once. (Sending the whole
// ~64KB base64 blob as one printf'd line was unreliable, which is why the
// frame is split into modest lines at all.)
constexpr size_t kChunkInputBytes = 384;
}  // namespace

SerialDumpDisplay::SerialDumpDisplay(FrameSize frame) : frame_(frame) {}

bool SerialDumpDisplay::init() {
    ESP_LOGI(kTag, "simulator backend ready (%zux%zu)", frame_.width, frame_.height);
    return true;
}

bool SerialDumpDisplay::clear() { return sendFramePacket({}); }

bool SerialDumpDisplay::flush(std::span<const uint8_t> framebuffer) { return sendFramePacket(framebuffer); }

void SerialDumpDisplay::sleep() { ESP_LOGI(kTag, "simulator backend sleep (no-op)"); }

bool SerialDumpDisplay::sendFramePacket(std::span<const uint8_t> framebuffer) {
    const size_t payloadSize = frame_.framebufferSize();
    const bool blank = framebuffer.empty();
    if (!blank && framebuffer.size() != payloadSize) {
        ESP_LOGE(kTag, "framebuffer is %zu bytes, expected %zu", framebuffer.size(), payloadSize);
        return false;
    }
    return sendPacket(kTypeFrame, payloadSize, [&](size_t i) -> uint8_t { return blank ? 0xFF : framebuffer[i]; });
}

bool SerialDumpDisplay::flushGray(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay) {
    if (framebuffer.size() != frame_.framebufferSize()) {
        ESP_LOGE(kTag, "framebuffer is %zu bytes, expected %zu", framebuffer.size(), frame_.framebufferSize());
        return false;
    }
    const size_t rowBytes = GrayBitmap::rowBytes(static_cast<int>(frame_.width));
    return sendPacket(kTypeGrayFrame, rowBytes * frame_.height, [&](size_t i) -> uint8_t {
        int y = static_cast<int>(i / rowBytes);
        int x0 = static_cast<int>(i % rowBytes) * 4;
        uint8_t byte = 0;
        for (int k = 0; k < 4; k++) {
            byte = static_cast<uint8_t>(byte << 2 | overlay.composedLevel(framebuffer, frame_, x0 + k, y));
        }
        return byte;
    });
}

bool SerialDumpDisplay::sendPacket(uint8_t type, size_t payloadSize,
                                   const std::function<uint8_t(size_t)>& payloadByte) {
    // [TYPE 1B][LENGTH 4B LE][PAYLOAD][CHECKSUM 2B LE], base64-encoded and
    // sent as a series of small plain-text lines (see display_sim.py for the
    // matching reassembly logic):
    //   GLNC:<seq>:<total>:<base64 chunk>
    // The checksum trails the payload, so it's summed as the bytes go out.
    const uint8_t header[kHeaderBytes] = {
        type,
        static_cast<uint8_t>(payloadSize),
        static_cast<uint8_t>(payloadSize >> 8),
        static_cast<uint8_t>(payloadSize >> 16),
        static_cast<uint8_t>(payloadSize >> 24),
    };
    uint16_t sum = 0;
    auto packetByte = [&](size_t i) -> uint8_t {
        if (i < kHeaderBytes) {
            return header[i];
        }
        i -= kHeaderBytes;
        if (i < payloadSize) {
            uint8_t b = payloadByte(i);
            sum = static_cast<uint16_t>(sum + b);
            return b;
        }
        return i == payloadSize ? static_cast<uint8_t>(sum) : static_cast<uint8_t>(sum >> 8);
    };

    const size_t packetSize = kHeaderBytes + payloadSize + kChecksumBytes;
    const size_t totalChunks = (packetSize + kChunkInputBytes - 1) / kChunkInputBytes;
    uint8_t input[kChunkInputBytes];
    unsigned char encoded[kChunkInputBytes / 3 * 4 + 1];  // + NUL, which mbedtls always writes
    for (size_t seq = 0; seq < totalChunks; seq++) {
        size_t offset = seq * kChunkInputBytes;
        size_t len = std::min(kChunkInputBytes, packetSize - offset);
        for (size_t i = 0; i < len; i++) {
            input[i] = packetByte(offset + i);  // in order, so the checksum is complete by the end
        }
        size_t written = 0;
        int ret = mbedtls_base64_encode(encoded, sizeof(encoded), &written, input, len);
        if (ret != 0) {
            ESP_LOGE(kTag, "base64 encode failed: %d", ret);
            return false;
        }
        printf("GLNC:%u:%u:%.*s\n", static_cast<unsigned>(seq), static_cast<unsigned>(totalChunks),
               static_cast<int>(written), encoded);
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
    return true;
}
