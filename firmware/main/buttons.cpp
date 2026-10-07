#include "buttons.hpp"

#include <ctime>

#include "app_state.hpp"
#include "board.hpp"
#include "button.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "peripherals.hpp"
#include "settings.hpp"

namespace glance {
namespace {
constexpr uint32_t kLongPressMs = 3000;
}  // namespace

void actOnPress(bool isLong, const std::function<void()>& waitForRelease) {
    ESP_LOGI(kTag, "button: %s press", isLong ? "long" : "short");
    if (gPhase == Phase::setup) {
        restartInto(false);
    }
    if (!isLong && (gBoot.offlineShown || gPhotoFrame)) {
        ESP_LOGI(kTag, "%s: short press ignored", gPhotoFrame ? "no calendars" : "offline");
        return;
    }
    // Lit for a short press's blink, and kept lit once a press has become a
    // long press, so you know when to let go.
    statusLedOn();
    if (isLong) {
        waitForRelease();
        restartInto(true);
    }
    vTaskDelay(pdMS_TO_TICKS(300));  // long enough to see the LED blink
    bool privacy = !settings::loadPrivacyMode();
    settings::savePrivacyMode(privacy);
    ESP_LOGI(kTag, "privacy mode %s", privacy ? "on" : "off");
    gBoot.pressed = true;
    restartInto(false);
}

void buttonTask(void*) {
    Button button(board::kButtonPin);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (!button.isDown()) {
            continue;
        }
        Button::Press press = button.readPress(kLongPressMs);
        if (press == Button::Press::none) {
            ESP_LOGI(kTag, "button: released within the debounce time, ignored");
            continue;
        }
        actOnPress(press == Button::Press::longPress, [&] {
            while (button.isDown()) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        });
    }
}

void actOnWakePress(bool noCalendars) {
    Button button(board::kButtonPin);
    while (button.isDown() && esp_timer_get_time() < static_cast<int64_t>(kLongPressMs) * 1000) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (button.isDown()) {
        ESP_LOGI(kTag, "button: long press (from wake-up)");
        statusLedOn();
        while (button.isDown()) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        gBoot.enterSetup = true;
        return;
    }
    ESP_LOGI(kTag, "button: short press (from wake-up)");
    if (gBoot.offlineShown || noCalendars) {
        if (gBoot.wakeAt > time(nullptr) + 60) {
            ESP_LOGI(kTag, "%s: back to sleep", noCalendars ? "no calendars" : "offline");
            logRun(wake_log::Result::backToSleep);
            deepSleep(gBoot.wakeAt);
        }
        return;  // the timer is due anyway: refresh
    }
    statusLedOn();
    bool privacy = !settings::loadPrivacyMode();
    settings::savePrivacyMode(privacy);
    ESP_LOGI(kTag, "privacy mode %s", privacy ? "on" : "off");
    gBoot.pressed = true;
    vTaskDelay(pdMS_TO_TICKS(300));  // long enough to see the LED blink
    statusLedOff();
}

}  // namespace glance
