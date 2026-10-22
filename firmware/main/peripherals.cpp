#include "peripherals.hpp"

#include <atomic>
#include <functional>
#include <vector>

#include "board.hpp"
#include "canvas.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.hpp"
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
    int64_t startUs = esp_timer_get_time();
    bool ready = display.init();
    int64_t readyUs = esp_timer_get_time();
    bool shown = ready && (overlay ? display.flushGray(framebuffer, *overlay) : display.flush(framebuffer));
    if (ready) {
        display.sleep();
    }
    // The flush includes the refresh, which the driver logs on its own.
    ESP_LOGI("panel", "init %lld ms, frame and refresh %lld ms", (readyUs - startUs) / 1000,
             (esp_timer_get_time() - readyUs) / 1000);
    return shown;
}

// Powers the panel up, hands it to `use`, then powers it down again; false
// (without touching the panel) once closePanel() has stopped drawing. The
// code around the backend selection is identical for both -- that's the
// point of the Display interface.
bool withPanel(const std::function<bool(Display&)>& use) {
    PanelState idle = PanelState::idle;
    if (!gPanel.compare_exchange_strong(idle, PanelState::drawing)) {
        return false;  // drawing already, or closed
    }
#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    SerialDumpDisplay display(board::kPanelSize);
    bool result = use(display);
#else
    Gpio peripheralPower(board::kPeripheralRailPin, Gpio::Direction::output);
    peripheralPower.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    bool result;
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
            .upsideDown = settings::loadUpsideDown(),
        };
        Epd7in5V2 display(epdConfig);
        result = use(display);
    }  // SPI bus released

    // Park the lines into the rail's chips low before cutting it: a line left
    // high feeds the unpowered rail through the chips' protection diodes
    // (the status LED on that rail was seen glowing dimly with it "off").
    for (int pin : {board::kEpdMosiPin, board::kEpdClkPin, board::kEpdCsPin, board::kEpdDcPin, board::kEpdResetPin}) {
        auto gpio = static_cast<gpio_num_t>(pin);
        gpio_reset_pin(gpio);
        gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(gpio, 0);
    }
    // Off again before deep sleep: the panel's circuit and the LED are on
    // this rail; with it cut the whole board sleeps at ~27uA.
    peripheralPower.write(false);
#endif
    gPanel = PanelState::idle;
    return result;
}

}  // namespace

bool show(std::span<const uint8_t> framebuffer, const GrayOverlay* overlay) {
    return withPanel([&](Display& display) { return drawOn(display, framebuffer, overlay); });
}

bool panelResponds() {
    return withPanel([](Display& display) {
        bool ready = display.init();
        if (ready) {
            display.sleep();
        }
        return ready;
    });
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

void blinkStatusLed() {
    statusLedOn();
    vTaskDelay(pdMS_TO_TICKS(150));
    statusLedOff();
}

void peripheralsOff() {
    gpio_set_direction(kStatusLed, GPIO_MODE_OUTPUT);
    gpio_set_level(kStatusLed, 0);  // parked low: nothing to feed back into the unpowered rail
    gpio_set_direction(kPeripheralRail, GPIO_MODE_OUTPUT);
    gpio_set_level(kPeripheralRail, 0);
}

}  // namespace glance
