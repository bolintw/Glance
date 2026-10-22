// Drains the battery at full load down to a resting voltage, then sleeps
// with everything off (see README.md). Full load: both cores spinning at
// 240MHz, Wi-Fi scanning nonstop with power save off, the peripheral rail
// on. Every DISCHARGE_LOAD_MINUTES the load stops for DISCHARGE_REST_SECONDS
// and the fuel gauge reads the battery: under load its voltage sags, which
// would stop the discharge early.
//
// LED: a short blink every 2s while discharging; five quick blinks, then
// off, once it's done (and again on a button wake-up if it's still low);
// three quick blinks every 2s if the fuel gauge doesn't answer.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "max17048.hpp"
#include "sdkconfig.h"

namespace {

constexpr const char* kTag = "discharge";
constexpr gpio_num_t kLed = static_cast<gpio_num_t>(CONFIG_DISCHARGE_LED_PIN);
constexpr gpio_num_t kRail = static_cast<gpio_num_t>(CONFIG_DISCHARGE_RAIL_PIN);
constexpr gpio_num_t kButton = GPIO_NUM_1;  // external pull-up, low when pressed; both boards
constexpr gpio_num_t kSda = GPIO_NUM_3;     // the gauge's bus, both boards
constexpr gpio_num_t kScl = GPIO_NUM_9;
// Never below this, whatever the target: the cell's protection cuts out
// around 2.7V, and a reading this low means something is off.
constexpr int kFloorMillivolts = 3000;

enum class Blink { slow, error };
std::atomic<Blink> gBlink{Blink::slow};
std::atomic<bool> gLoad{false};
bool gWifiStarted = false;

void led(bool on) { gpio_set_level(kLed, on ? 0 : 1); }  // active low

void blinkQuickly(int times) {
    for (int i = 0; i < times; i++) {
        led(true);
        vTaskDelay(pdMS_TO_TICKS(120));
        led(false);
        vTaskDelay(pdMS_TO_TICKS(180));
    }
}

void ledTask(void*) {
    while (true) {
        if (gBlink == Blink::error) {
            blinkQuickly(3);
            vTaskDelay(pdMS_TO_TICKS(1100));
        } else {
            led(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            led(false);
            vTaskDelay(pdMS_TO_TICKS(1900));
        }
    }
}

// Spins while the load is on. One per core, above the idle tasks.
void burnTask(void*) {
    volatile uint32_t x = 1;
    while (true) {
        if (!gLoad) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        for (int i = 0; i < 100000; i++) {
            x = x * 1664525u + 1013904223u;
        }
    }
}

// Active scans nonstop: probe requests on every channel, the receiver on
// in between.
void scanTask(void*) {
    while (true) {
        if (!gLoad) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (esp_wifi_scan_start(nullptr, true) == ESP_OK) {
            esp_wifi_clear_ap_list();
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void startWifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = 0;  // nothing of the tool's goes into the device's NVS
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}

void loadOn() {
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    gWifiStarted = true;
    gLoad = true;
}

void loadOff() {
    gLoad = false;
    if (gWifiStarted) {
        esp_wifi_scan_stop();
        esp_wifi_stop();
        gWifiStarted = false;
    }
}

class Gauge {
public:
    Gauge() {
        i2c_master_bus_config_t bus = {};
        bus.i2c_port = I2C_NUM_0;
        bus.sda_io_num = kSda;
        bus.scl_io_num = kScl;
        bus.clk_source = I2C_CLK_SRC_DEFAULT;
        bus.glitch_ignore_cnt = 7;
        if (i2c_new_master_bus(&bus, &bus_) != ESP_OK) {
            return;
        }
        i2c_device_config_t device = {};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = max17048::kAddress;
        device.scl_speed_hz = 100'000;
        i2c_master_bus_add_device(bus_, &device, &device_);
    }

    std::optional<uint16_t> read(uint8_t reg) {
        uint8_t data[2];
        if (!device_ || i2c_master_transmit_receive(device_, &reg, 1, data, sizeof(data), 50) != ESP_OK) {
            return std::nullopt;
        }
        return static_cast<uint16_t>(data[0] << 8 | data[1]);
    }

    bool write(uint8_t reg, uint16_t value) {
        uint8_t data[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
        return device_ && i2c_master_transmit(device_, data, sizeof(data), 50) == ESP_OK;
    }

    bool present() {
        auto version = read(max17048::kVersion);
        return version && max17048::isVersion(*version);
    }

    // The average of four samples: in active mode the gauge updates VCELL
    // every 250ms.
    std::optional<int> millivolts() {
        int sum = 0;
        for (int i = 0; i < 4; i++) {
            auto vcell = read(max17048::kVcell);
            if (!vcell) {
                return std::nullopt;
            }
            sum += max17048::millivolts(*vcell);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        return sum / 4;
    }

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t device_ = nullptr;
};

void outputs() {
    for (gpio_num_t pin : {kLed, kRail}) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    }
    led(false);
    gpio_set_level(kRail, 1);  // the LED is on this rail, and the panel's circuit is load too
}

// Done (or can't go on): everything off, the gauge back to always
// hibernating as the firmware keeps it, and asleep until the button.
[[noreturn]] void finish(Gauge& gauge, bool showDone) {
    loadOff();
    if (showDone) {
        blinkQuickly(5);
    }
    gauge.write(max17048::kHibrt, max17048::kHibrtAlways);
    gpio_set_level(kRail, 0);
    gpio_set_level(kLed, 0);  // parked low once the rail is off: nothing to feed back into it
    esp_sleep_enable_ext1_wakeup(1ULL << kButton, ESP_EXT1_WAKEUP_ANY_LOW);
    ESP_LOGI(kTag, "asleep until the button");
    esp_deep_sleep_start();
}

// Load steps for comparing boards (DISCHARGE_STEPS): 10s each, forever.
[[noreturn]] void runSteps() {
    gpio_set_level(kRail, 0);
    xTaskCreatePinnedToCore(burnTask, "burn0", 2048, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(burnTask, "burn1", 2048, nullptr, 1, nullptr, 1);
    startWifi();
    xTaskCreate(scanTask, "scan", 4096, nullptr, 2, nullptr);
    constexpr TickType_t kStep = pdMS_TO_TICKS(10 * 1000);
    while (true) {
        ESP_LOGI(kTag, "step 1: idle");
        loadOff();
        vTaskDelay(kStep);
        ESP_LOGI(kTag, "step 2: CPU");
        gLoad = true;  // the burn tasks; Wi-Fi isn't started, so the scan task waits
        vTaskDelay(kStep);
        ESP_LOGI(kTag, "step 3: CPU + Wi-Fi");
        gLoad = false;
        loadOn();
        vTaskDelay(kStep);
    }
}

}  // namespace

extern "C" void app_main(void) {
    outputs();
#if CONFIG_DISCHARGE_STEPS
    runSteps();
#endif
    Gauge gauge;
    if (!gauge.present()) {
        ESP_LOGE(kTag, "fuel gauge not answering");
        gBlink = Blink::error;
        xTaskCreate(ledTask, "led", 2048, nullptr, 5, nullptr);
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
        finish(gauge, false);
    }
    // Active mode: a fresh VCELL every 250ms rather than every 45s.
    gauge.write(max17048::kHibrt, 0x0000);
    const int target = std::max(CONFIG_DISCHARGE_TARGET_MV, kFloorMillivolts);
    ESP_LOGI(kTag, "target %d mV at rest; %d min load, %d s rest", target, CONFIG_DISCHARGE_LOAD_MINUTES,
             CONFIG_DISCHARGE_REST_SECONDS);

    // At power-up the cell has been resting already; a moment for the gauge.
    vTaskDelay(pdMS_TO_TICKS(1000));
    auto mv = gauge.millivolts();
    if (mv && *mv <= target) {
        ESP_LOGI(kTag, "already at %d mV", *mv);
        finish(gauge, true);
    }

    xTaskCreate(ledTask, "led", 2048, nullptr, 5, nullptr);
    xTaskCreatePinnedToCore(burnTask, "burn0", 2048, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(burnTask, "burn1", 2048, nullptr, 1, nullptr, 1);
    startWifi();
    xTaskCreate(scanTask, "scan", 4096, nullptr, 2, nullptr);

    while (true) {
        loadOn();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DISCHARGE_LOAD_MINUTES * 60 * 1000));
        loadOff();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DISCHARGE_REST_SECONDS * 1000));
        mv = gauge.millivolts();
        if (!mv) {
            ESP_LOGE(kTag, "fuel gauge stopped answering");
            finish(gauge, false);
        }
        ESP_LOGI(kTag, "%d mV at rest", *mv);
        if (*mv <= target) {
            finish(gauge, true);
        }
    }
}
