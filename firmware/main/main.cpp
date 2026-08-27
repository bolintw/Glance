#include <algorithm>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
#include "serial_dump_display.hpp"
#else
#include "driver/spi_master.h"
#include "epd_7in5_v2.hpp"
#include "gpio.hpp"
#include "spi.hpp"
#endif

namespace {
constexpr const char* kTag = "main";
constexpr FrameSize kPanelSize{.width = 800, .height = 480};

#ifndef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
// GPIO14: switches the V33_2 rail that powers the EPD, external flash, and
// LED. It's off by default to save power in deep sleep -- must be driven
// high before the EPD (or anything else on that rail) will respond to
// anything, including SPI commands and the busy pin.
constexpr int kPeripheralPowerPin = 14;

constexpr int kEpdMosiPin = 35;
constexpr int kEpdClkPin = 36;
constexpr int kEpdCsPin = 37;
constexpr int kEpdDcPin = 38;
constexpr int kEpdResetPin = 39;
constexpr int kEpdBusyPin = 40;
constexpr uint32_t kEpdSpiClockHz = 1'000'000;
#endif

// M1 EPD regression test pattern: a border frame plus a diagonal line.
// Deliberately simple/procedural -- no fonts or images yet, this only
// exists to prove the ported driver's bit packing and full-refresh timing
// are correct. Bit 1 == white, 0 == black (see display.hpp).
[[maybe_unused]] std::vector<uint8_t> buildTestPattern(const FrameSize& frame) {
    std::vector<uint8_t> framebuffer(frame.framebufferSize(), 0xFF);
    size_t bytesPerRow = frame.bytesPerRow();

    auto setBlack = [&](size_t x, size_t y) {
        size_t byteIndex = y * bytesPerRow + x / 8;
        uint8_t bitMask = static_cast<uint8_t>(0x80 >> (x % 8));
        framebuffer[byteIndex] &= static_cast<uint8_t>(~bitMask);
    };

    for (size_t x = 0; x < frame.width; x++) {
        setBlack(x, 0);
        setBlack(x, frame.height - 1);
    }
    for (size_t y = 0; y < frame.height; y++) {
        setBlack(0, y);
        setBlack(frame.width - 1, y);
    }
    for (size_t x = 0; x < frame.width; x++) {
        size_t y = x * frame.height / frame.width;
        setBlack(x, y);
    }

    return framebuffer;
}

// Hardware-and-agent-in-the-loop demo: a circle outline, drawn with the
// midpoint circle algorithm, centered on the panel.
std::vector<uint8_t> buildCirclePattern(const FrameSize& frame) {
    std::vector<uint8_t> framebuffer(frame.framebufferSize(), 0xFF);
    size_t bytesPerRow = frame.bytesPerRow();

    auto setBlack = [&](int x, int y) {
        if (x < 0 || y < 0 || static_cast<size_t>(x) >= frame.width || static_cast<size_t>(y) >= frame.height) {
            return;
        }
        size_t byteIndex = static_cast<size_t>(y) * bytesPerRow + static_cast<size_t>(x) / 8;
        uint8_t bitMask = static_cast<uint8_t>(0x80 >> (x % 8));
        framebuffer[byteIndex] &= static_cast<uint8_t>(~bitMask);
    };

    int centerX = static_cast<int>(frame.width / 2);
    int centerY = static_cast<int>(frame.height / 2);
    int radius = static_cast<int>(std::min(frame.width, frame.height) / 2) - 20;

    auto plotOctants = [&](int x, int y) {
        setBlack(centerX + x, centerY + y);
        setBlack(centerX - x, centerY + y);
        setBlack(centerX + x, centerY - y);
        setBlack(centerX - x, centerY - y);
        setBlack(centerX + y, centerY + x);
        setBlack(centerX - y, centerY + x);
        setBlack(centerX + y, centerY - x);
        setBlack(centerX - y, centerY - x);
    };

    int x = 0;
    int y = radius;
    int d = 1 - radius;
    plotOctants(x, y);
    while (x < y) {
        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
        plotOctants(x, y);
    }

    return framebuffer;
}
}  // namespace

extern "C" void app_main(void)
{
#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    SerialDumpDisplay display(kPanelSize);
#else
    Gpio peripheralPower(kPeripheralPowerPin, Gpio::Direction::output);
    peripheralPower.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    SpiConfig spiConfig{
        .mosiPin = kEpdMosiPin,
        .misoPin = -1,
        .sclkPin = kEpdClkPin,
        .csPin = kEpdCsPin,
        .hostId = SPI2_HOST,
        .clockSpeedHz = kEpdSpiClockHz,
        .maxTransferSize = kPanelSize.framebufferSize(),
    };
    Spi spi(spiConfig);

    EpdConfig epdConfig{
        .frame = kPanelSize,
        .spiDevice = spi,
        .dcPin = kEpdDcPin,
        .resetPin = kEpdResetPin,
        .busyPin = kEpdBusyPin,
    };
    Epd7in5V2 display(epdConfig);
#endif

    // Everything below is identical regardless of backend -- that's the
    // point of the Display interface (M2).
    ESP_LOGI(kTag, "M1 EPD regression: init + clear");
    display.init();
    display.clear();

    ESP_LOGI(kTag, "M1 EPD regression: flushing test pattern");
    auto pattern = buildCirclePattern(kPanelSize);
    display.flush(pattern);

    ESP_LOGI(kTag, "M1 EPD regression: sleep");
    display.sleep();
}
