#include "epd_7in5_v2.hpp"

#include <cstring>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr const char* kTag = "epd_7in5_v2";

void delayMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
}  // namespace

Epd7in5V2::Epd7in5V2(const EpdConfig& config)
    : frame_(config.frame),
      spi_(config.spiDevice),
      dc_(config.dcPin, Gpio::Direction::output),
      reset_(config.resetPin, Gpio::Direction::output),
      // Pulled down so a missing panel reads as never idle instead of
      // always idle (see kPresenceTimeoutMs). There's no external pull on
      // the board.
      busy_(config.busyPin, Gpio::Direction::input, Gpio::Pull::down) {}

bool Epd7in5V2::init() {
    resetHardware();
    if (!waitUntilIdle(kPresenceTimeoutMs)) {
        ESP_LOGE(kTag, "panel not detected (BUSY stayed low after reset) -- check the FPC cable");
        return false;
    }

    setBoosterSoftStart();
    setPowerSetting();

    sendCommand(epd::Command::PON);
    delayMs(100);
    if (!waitUntilIdle()) {
        ESP_LOGE(kTag, "panel did not finish powering on");
        return false;
    }

    setPanelSetting();
    setResolutionSetting();

    sendCommand(epd::Command::DUSPI);
    sendData(0x00);

    setVcomAndDataInterval();
    setTconSetting();
    return true;
}

bool Epd7in5V2::clear() {
    std::vector<uint8_t> allOnes(frame_.bytesPerRow(), 0xFF);
    std::vector<uint8_t> allZeros(frame_.bytesPerRow(), 0x00);

    sendCommand(epd::Command::DTM1);
    for (size_t row = 0; row < frame_.height; row++) {
        sendData(allOnes);
    }

    sendCommand(epd::Command::DTM2);
    for (size_t row = 0; row < frame_.height; row++) {
        sendData(allZeros);
    }

    return turnOnDisplay();
}

bool Epd7in5V2::flush(std::span<const uint8_t> framebuffer) {
    size_t bytesPerRow = frame_.bytesPerRow();

    sendCommand(epd::Command::DTM1);
    for (size_t row = 0; row < frame_.height; row++) {
        sendData(framebuffer.subspan(row * bytesPerRow, bytesPerRow));
    }

    std::vector<uint8_t> invertedRow(bytesPerRow);
    sendCommand(epd::Command::DTM2);
    for (size_t row = 0; row < frame_.height; row++) {
        auto sourceRow = framebuffer.subspan(row * bytesPerRow, bytesPerRow);
        for (size_t i = 0; i < bytesPerRow; i++) {
            invertedRow[i] = static_cast<uint8_t>(~sourceRow[i]);
        }
        sendData(invertedRow);
    }

    return turnOnDisplay();
}

bool Epd7in5V2::flushGray(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay) {
    // 4-gray mode needs its own register setup, so start over from a reset.
    resetHardware();
    if (!waitUntilIdle(kPresenceTimeoutMs)) {
        ESP_LOGE(kTag, "panel not detected (BUSY stayed low after reset) -- check the FPC cable");
        return false;
    }
    initGrayMode();
    sendGrayPlane(framebuffer, overlay, false);
    sendGrayPlane(framebuffer, overlay, true);
    return turnOnDisplay();
}

// Waveshare's EPD_7IN5_V2_Init_4Gray. The 0x5F "forced temperature" selects
// the 4-gray waveform stored in the panel's OTP.
void Epd7in5V2::initGrayMode() {
    setPanelSetting();
    setResolutionSetting();  // not in Waveshare's sequence, but it's what the reset default should be anyway
    sendCommand(epd::Command::CDI);
    sendData(epd::Config::Border::kBlack);
    sendData(0x07);
    sendCommand(epd::Command::PON);
    delayMs(100);
    if (!waitUntilIdle()) {
        ESP_LOGE(kTag, "panel did not finish powering on");
    }
    sendCommand(epd::Command::BTST);
    sendData(0x27);
    sendData(0x27);
    sendData(0x18);
    sendData(0x17);
    sendCommand(epd::Command::CCSET);
    sendData(0x02);
    sendCommand(epd::Command::TSSET);
    sendData(0x5F);
}

// Each pixel's level is split across the two planes (from Waveshare's
// EPD_7IN5_V2_Display_4Gray; a set bit drives toward black):
//   level      3 white   2 light   1 dark   0 black
//   DTM1         0         1         0        1
//   DTM2         0         0         1        1
void Epd7in5V2::sendGrayPlane(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay, bool second) {
    sendCommand(second ? epd::Command::DTM2 : epd::Command::DTM1);
    std::vector<uint8_t> row(frame_.bytesPerRow());
    for (size_t y = 0; y < frame_.height; y++) {
        for (size_t byte = 0; byte < row.size(); byte++) {
            uint8_t bits = 0;
            for (int bit = 0; bit < 8; bit++) {
                int x = static_cast<int>(byte * 8) + bit;
                uint8_t level = overlay.composedLevel(framebuffer, frame_, x, static_cast<int>(y));
                bool set = second ? level <= 1 : level % 2 == 0;
                if (set) {
                    bits |= 0x80 >> bit;
                }
            }
            row[byte] = bits;
        }
        sendData(row);
    }
}

void Epd7in5V2::sleep() {
    sendCommand(epd::Command::CDI);
    sendData(epd::Config::Border::kFloating);
    sendCommand(epd::Command::POF);
    waitUntilIdle();
    sendCommand(epd::Command::DSLP);
    sendData(epd::Config::Sleep::kKeepRam);
}

void Epd7in5V2::setBoosterSoftStart() {
    sendCommand(epd::Command::BTST);
    sendData(epd::Config::Booster::kPhaseA);
    sendData(epd::Config::Booster::kPhaseB);
    sendData(epd::Config::Booster::kPhaseC);
    sendData(epd::Config::Booster::kPhaseD);
}

void Epd7in5V2::setPowerSetting() {
    sendCommand(epd::Command::PWR);
    sendData(epd::Config::Power::kVgh20V);
    sendData(epd::Config::Power::kVglNeg20V);
    sendData(epd::Config::Power::kVdh15V);
    sendData(epd::Config::Power::kVdlNeg15V);
}

void Epd7in5V2::setPanelSetting() {
    sendCommand(epd::Command::PSR);
    sendData(epd::Config::Panel::kScanUp | epd::Config::Panel::kShiftRight |
              epd::Config::Panel::kBoosterOn | epd::Config::Panel::kNoReset | 0x01);
}

void Epd7in5V2::setResolutionSetting() {
    sendCommand(epd::Command::TRES);
    sendData(static_cast<uint8_t>((frame_.width >> 8) & 0x03));
    sendData(static_cast<uint8_t>(frame_.width & 0xFF));
    sendData(static_cast<uint8_t>((frame_.height >> 8) & 0x01));
    sendData(static_cast<uint8_t>(frame_.height & 0xFF));
}

void Epd7in5V2::setVcomAndDataInterval() {
    sendCommand(epd::Command::CDI);
    sendData(epd::Config::Border::kBlack);
    sendData(0x07);
}

void Epd7in5V2::setTconSetting() {
    sendCommand(epd::Command::TCON);
    sendData(0x22);
}

void Epd7in5V2::sendCommand(epd::Command cmd) {
    dc_.write(false);
    uint8_t cmdByte = static_cast<uint8_t>(cmd);
    spi_.write(std::span<const uint8_t>(&cmdByte, 1));
}

void Epd7in5V2::sendData(uint8_t data) {
    dc_.write(true);
    spi_.write(std::span<const uint8_t>(&data, 1));
}

void Epd7in5V2::sendData(std::span<const uint8_t> data) {
    dc_.write(true);
    spi_.write(data);
}

bool Epd7in5V2::waitUntilIdle(uint32_t timeoutMs) {
    // Tracks real elapsed time via esp_timer rather than counting nominal
    // delay values: at the default 100Hz FreeRTOS tick rate, a nominal 5ms
    // delay rounds down to 0 ticks (pdMS_TO_TICKS truncates), so counting
    // "5ms per iteration" would report a 10s timeout after under a second
    // of actual wall-clock time.
    int64_t deadlineUs = esp_timer_get_time() + static_cast<int64_t>(timeoutMs) * 1000;
    while (!busy_.read()) {
        sendCommand(epd::Command::FLG);
        delayMs(10);
        if (esp_timer_get_time() >= deadlineUs) {
            ESP_LOGE(kTag, "busy pin timeout after %lums -- check EPD_BSY wiring",
                     static_cast<unsigned long>(timeoutMs));
            return false;
        }
    }
    delayMs(20);
    return true;
}

bool Epd7in5V2::waitUntilBusy(uint32_t timeoutMs) {
    int64_t deadlineUs = esp_timer_get_time() + static_cast<int64_t>(timeoutMs) * 1000;
    while (busy_.read()) {
        if (esp_timer_get_time() >= deadlineUs) {
            return false;
        }
        delayMs(10);
    }
    return true;
}

bool Epd7in5V2::turnOnDisplay() {
    int64_t startUs = esp_timer_get_time();
    sendCommand(epd::Command::DRF);
    if (!waitUntilBusy(kRefreshStartTimeoutMs)) {
        ESP_LOGE(kTag, "panel did not start refreshing (BUSY never went low)");
        return false;
    }
    if (!waitUntilIdle()) {
        ESP_LOGE(kTag, "display refresh did not complete");
        return false;
    }
    ESP_LOGI(kTag, "refresh took %lldms", (esp_timer_get_time() - startUs) / 1000);
    return true;
}

void Epd7in5V2::resetHardware() {
    // The panel's own reference driver uses 20ms/2ms/20ms here (see
    // refs/private/datasheets/waveshare_epd_7in5_v2_reference/EPD_7in5_V2.c).
    // On this board that timing was found unreliable -- Clear() would
    // intermittently fail to take effect -- so this holds each phase 10x
    // longer. Confirmed against git history (every past porting attempt on
    // this hardware independently converged on the same 200/200/200ms).
    reset_.write(true);
    delayMs(200);
    reset_.write(false);
    delayMs(200);
    reset_.write(true);
    delayMs(200);
}
