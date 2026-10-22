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
#include "run_policy.hpp"
#include "settings.hpp"

namespace glance {
namespace {
constexpr uint32_t kLongPressMs = 3000;

// Waits for `button`, down since `downAtUs` (esp_timer time) with the LED
// lit, to come up: for at most kStuckButtonMs from then. Once it's been held
// to kEraseHoldMs the LED blinks fast: letting go now erases everything.
Hold waitForRelease(const Button& button, int64_t downAtUs) {
    const int64_t eraseAt = downAtUs + static_cast<int64_t>(kEraseHoldMs) * 1000;
    const int64_t deadline = downAtUs + static_cast<int64_t>(kStuckButtonMs) * 1000;
    while (button.isDown()) {
        const int64_t now = esp_timer_get_time();
        if (now > deadline) {
            ESP_LOGW(kTag, "button held for %lu s: stuck, ignoring it", static_cast<unsigned long>(kStuckButtonMs / 1000));
            statusLedOff();
            return Hold::stuck;
        }
        if (now >= eraseAt) {
            ((now - eraseAt) / 100000 % 2 ? statusLedOff : statusLedOn)();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return esp_timer_get_time() >= eraseAt ? Hold::heldToErase : Hold::released;
}

// A long press was let go: what it asked for. In setup mode it leaves setup;
// otherwise it opens it.
void afterLongPress(Hold hold, bool intoSetup) {
    switch (hold) {
        case Hold::released:
            restartInto(intoSetup);
        case Hold::heldToErase:
            ESP_LOGW(kTag, "button held for %lu s: erasing everything", static_cast<unsigned long>(kEraseHoldMs / 1000));
            restartToErase();
        case Hold::stuck:
            statusLedOff();
            return;
    }
}
}  // namespace

void actOnPress(bool isLong, const std::function<Hold()>& waitForRelease) {
    ESP_LOGI(kTag, "button: %s press", isLong ? "long" : "short");
    switch (gPhase.load()) {
        case Phase::setupStarting:
            // Its screen is on the way (a few seconds of WiFi and panel):
            // pressing again would only start it over, again and again.
            blinkStatusLed();
            ESP_LOGI(kTag, "setup starting: press ignored");
            return;
        case Phase::setupClosed:
            restartInto(false);  // a second device closed setup: start over
        case Phase::setup:
            if (!isLong) {
                blinkStatusLed();
                ESP_LOGI(kTag, "setup: short press ignored");
                return;
            }
            // A long press leaves setup: back to the calendar, or on a first
            // setup, a fresh start (a new password).
            statusLedOn();
            afterLongPress(waitForRelease(), false);
            return;
        case Phase::refreshing:
        case Phase::waiting:
            break;
    }
    if (!isLong && (gBoot.offlineShown || gPhotoFrame)) {
        ESP_LOGI(kTag, "%s: short press ignored", gPhotoFrame ? "no calendars" : "offline");
        return;
    }
    // Lit for a short press's blink, and kept lit once a press has become a
    // long press, so you know when to let go.
    statusLedOn();
    if (isLong) {
        afterLongPress(waitForRelease(), true);
        return;
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
        const int64_t downAt = esp_timer_get_time();
        Button::Press press = button.readPress(kLongPressMs);
        if (press == Button::Press::none) {
            ESP_LOGI(kTag, "button: released within the debounce time, ignored");
            continue;
        }
        actOnPress(press == Button::Press::longPress, [&] { return waitForRelease(button, downAt); });
        // A stuck button is still down: don't take it for a new press each time round.
        while (button.isDown()) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void actOnWakePress(bool noCalendars) {
    Button button(board::kButtonPin);
    while (button.isDown() && esp_timer_get_time() < static_cast<int64_t>(kLongPressMs) * 1000) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    const bool held = button.isDown();
    Hold hold = Hold::released;
    if (held) {
        ESP_LOGI(kTag, "button: long press (from wake-up)");
        statusLedOn();
        hold = waitForRelease(button, 0);  // down since the wake-up
    } else {
        ESP_LOGI(kTag, "button: short press (from wake-up)");
        // The press got through: say so at once. Whatever it leads to takes
        // seconds to reach the panel, and without this people press again.
        blinkStatusLed();
    }
    switch (run_policy::decideWakePress(held, hold != Hold::stuck, hold == Hold::heldToErase,
                                        gBoot.offlineShown || noCalendars, gBoot.wakeAt, time(nullptr))) {
        case run_policy::WakePress::setup:
            gBoot.enterSetup = true;
            return;
        case run_policy::WakePress::erase:
            ESP_LOGW(kTag, "button held for %lu s: erasing everything", static_cast<unsigned long>(kEraseHoldMs / 1000));
            restartToErase();
        case run_policy::WakePress::stuck:
            statusLedOff();
            return;  // carry on with the run as if it hadn't been pressed
        case run_policy::WakePress::backToSleep:
            ESP_LOGI(kTag, "%s: back to sleep", noCalendars ? "no calendars" : "offline");
            logRun(wake_log::Result::backToSleep);
            deepSleep(gBoot.wakeAt);
        case run_policy::WakePress::refresh:
            return;  // the timer is due anyway
        case run_policy::WakePress::togglePrivacy:
            break;
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
