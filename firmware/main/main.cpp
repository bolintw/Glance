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
std::vector<uint8_t> buildTestPattern(const FrameSize& frame) {
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
    auto pattern = buildTestPattern(kPanelSize);
    display.flush(pattern);

    ESP_LOGI(kTag, "M1 EPD regression: sleep");
    display.sleep();
}
