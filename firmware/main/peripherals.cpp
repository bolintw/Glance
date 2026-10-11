#include "peripherals.hpp"

#include <atomic>
#include <vector>

#include "board.hpp"
#include "canvas.hpp"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "setup_view.hpp"

#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
#include "serial_dump_display.hpp"
#else
#include "driver/spi_master.h"
#include "epd_7in5_v2.hpp"
#include "gpio.hpp"
#include "spi.hpp"
#endif

namespace glance {
namespace {

constexpr gpio_num_t kStatusLed = static_cast<gpio_num_t>(board::kStatusLedPin);
constexpr gpio_num_t kPeripheralRail = static_cast<gpio_num_t>(board::kPeripheralRailPin);

// Whether show() is drawing, and whether closePanel() has stopped it for good.
enum class PanelState { idle, drawing, closed };
std::atomic<PanelState> gPanel = PanelState::idle;

// init -> one frame -> sleep, on whichever backend. False if the panel
// didn't respond.
bool drawOn(Display& display, std::span<const uint8_t> framebuffer, const GrayOverlay* overlay) {
    // No clear() first: a full refresh already drives every pixel through
    // the whole waveform, so clearing only doubled the time and flicker.
    bool ready = display.init();
    bool shown = ready && (overlay ? display.flushGray(framebuffer, *overlay) : display.flush(framebuffer));
    if (ready) {
        display.sleep();
    }
    return shown;
}

}  // namespace

// The code below the backend selection is identical for both -- that's the
// point of the Display interface.
bool show(std::span<const uint8_t> framebuffer, const GrayOverlay* overlay) {
    PanelState idle = PanelState::idle;
    if (!gPanel.compare_exchange_strong(idle, PanelState::drawing)) {
        return false;  // closed
    }
#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    SerialDumpDisplay display(board::kPanelSize);
    bool shown = drawOn(display, framebuffer, overlay);
    gPanel = PanelState::idle;
    return shown;
#else
    Gpio peripheralPower(board::kPeripheralRailPin, Gpio::Direction::output);
    peripheralPower.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    bool shown;
    {
        SpiConfig spiConfig{
            .mosiPin = board::kEpdMosiPin,
            .misoPin = -1,
            .sclkPin = board::kEpdClkPin,
            .csPin = board::kEpdCsPin,
            .hostId = SPI2_HOST,
            .clockSpeedHz = board::kEpdSpiClockHz,
            .maxTransferSize = board::kPanelSize.framebufferSize(),
        };
        Spi spi(spiConfig);

        EpdConfig epdConfig{
            .frame = board::kPanelSize,
            .spiDevice = spi,
            .dcPin = board::kEpdDcPin,
            .resetPin = board::kEpdResetPin,
            .busyPin = board::kEpdBusyPin,
        };
        Epd7in5V2 display(epdConfig);
        shown = drawOn(display, framebuffer, overlay);
    }  // SPI bus released

    // Park the lines into the rail's chips low before cutting it: a line left
    // high feeds the unpowered rail through the chips' protection diodes
    // (the status LED on that rail was seen glowing dimly with it "off").
    for (int pin : {board::kEpdMosiPin, board::kEpdClkPin, board::kEpdCsPin, board::kEpdDcPin, board::kEpdResetPin,
                    board::kFlashCsPin}) {
        auto gpio = static_cast<gpio_num_t>(pin);
        gpio_reset_pin(gpio);
        gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(gpio, 0);
    }
    // Off again before deep sleep: with the external flash powered, its SPI
    // lines leak ~300uA; with the rail cut the whole board sleeps at ~70uA.
    peripheralPower.write(false);
    gPanel = PanelState::idle;
    return shown;
#endif
}

bool closePanel(uint32_t waitMs) {
    for (uint32_t waited = 0;; waited += 100) {
        PanelState idle = PanelState::idle;
        if (gPanel.compare_exchange_strong(idle, PanelState::closed)) {
            return true;
        }
        if (waited >= waitMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void showNotice(std::string_view title, std::span<const std::string_view> lines) {
    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    setup_view::renderNotice(canvas, title, lines);
    show(framebuffer);
}

// Raw driver calls rather than Gpio: Gpio's constructor resets the pin,
// which would cut the rail under a panel refresh in progress.
void statusLedOn() {
    gpio_set_direction(kPeripheralRail, GPIO_MODE_OUTPUT);
    gpio_set_level(kPeripheralRail, 1);
    gpio_reset_pin(kStatusLed);
    gpio_set_direction(kStatusLed, GPIO_MODE_OUTPUT);
    gpio_set_level(kStatusLed, 0);  // active low
}

void statusLedOff() { gpio_set_level(kStatusLed, 1); }

void peripheralsOff() {
    gpio_set_direction(kStatusLed, GPIO_MODE_OUTPUT);
    gpio_set_level(kStatusLed, 0);  // parked low: nothing to feed back into the unpowered rail
    gpio_set_direction(kPeripheralRail, GPIO_MODE_OUTPUT);
    gpio_set_level(kPeripheralRail, 0);
}

}  // namespace glance
