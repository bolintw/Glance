#include "button.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr int64_t kDebounceUs = 30 * 1000;
}

Button::Button(uint8_t pin) : pin_(pin, Gpio::Direction::input) {}

bool Button::isDown() const { return !pin_.read(); }

Button::Press Button::readPress(uint32_t longPressMs) const {
    // Down must hold for the debounce time, release likewise.
    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < kDebounceUs) {
        if (!isDown()) {
            return Press::none;
        }
        vTaskDelay(1);
    }
    int64_t upSince = 0;
    while (true) {
        int64_t now = esp_timer_get_time();
        if (now - start >= static_cast<int64_t>(longPressMs) * 1000) {
            return Press::longPress;
        }
        if (isDown()) {
            upSince = 0;
        } else if (upSince == 0) {
            upSince = now;
        } else if (now - upSince >= kDebounceUs) {
            return Press::shortPress;
        }
        vTaskDelay(1);
    }
}
