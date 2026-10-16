#include "app_state.hpp"

#include <algorithm>
#include <ctime>

#include "board.hpp"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_update.hpp"
#include "time_sync.hpp"
#include "wake_log_store.hpp"

namespace glance {

RTC_NOINIT_ATTR BootState gBoot;
std::atomic<Phase> gPhase{Phase::refreshing};
std::atomic<bool> gPhotoFrame{false};
bool gLowBattery = false;
wake_log::Entry gRun;

void initBootState() {
    if (gBoot.magic == BootState::kMagic) {
        return;
    }
    gBoot = {.magic = BootState::kMagic,
             .enterSetup = false,
             .accessPointSetup = false,
             .wifiFailed = false,
             .justConfigured = false,
             .homeStepNext = false,
             .pressed = false,
             .offlineShown = false,
             .batteryCritical = false,
             .wifiFailures = 0,
             .failedRuns = 0,
             .badResets = 0,
             .lastPhoto = UINT8_MAX,
             .wakeAt = 0,
             .fakeBatteryPct = -1,
             .forecastAt = 0,
             .forecastCondition = 0,
             .forecastRainChance = 0,
             .forecastMinTemp = 0,
             .forecastMaxTemp = 0};
}

void logRun(wake_log::Result result) {
    gRun.result = result;
    gRun.awakeMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    gRun.time = time_sync::clockIsPlausible() ? static_cast<uint32_t>(time(nullptr)) : 0;
    esp_err_t err = wake_log_store::append(gRun);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "wake log not written: %s", esp_err_to_name(err));
    }
}

[[noreturn]] void restartInto(bool enterSetup) {
    ota_update::markRunningAppValid();  // a deliberate restart isn't a failed update
    gBoot.enterSetup = enterSetup;
    esp_restart();
}

[[noreturn]] void deepSleep(std::optional<int64_t> wakeAt, bool buttonOnExt0) {
    esp_wifi_stop();  // harmless if WiFi never started
    // The button wakes on a low level, so one that's stuck down would wake the
    // device the moment it slept, over and over: then only the timer does --
    // in an hour, if nothing else was due.
    const auto button = static_cast<gpio_num_t>(board::kButtonPin);
    gpio_set_direction(button, GPIO_MODE_INPUT);  // may not be set up yet this run (the external pull-up holds it)
    const bool buttonDown = gpio_get_level(button) == 0;
    if (buttonDown && !wakeAt) {
        wakeAt = time(nullptr) + 3600;
    }
    gBoot.wakeAt = wakeAt.value_or(0);
    if (buttonDown) {
        ESP_LOGW(kTag, "button is down going to sleep (stuck?): waking on the timer only");
    } else if (buttonOnExt0) {
        esp_sleep_enable_ext0_wakeup(button, 0);
    } else {
        esp_sleep_enable_ext1_wakeup_io(BIT64(board::kButtonPin), ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (wakeAt) {
        int64_t seconds = std::max<int64_t>(*wakeAt - time(nullptr), 1);
        esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000);
    }
    // Give the USB serial port a moment to send the last log lines; deep
    // sleep cuts it off mid-buffer otherwise.
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}

}  // namespace glance
