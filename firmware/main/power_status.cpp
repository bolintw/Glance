#include "power_status.hpp"

#include "board.hpp"
#include "sdkconfig.h"

#if CONFIG_GLANCE_BOARD_V3_0

#include <optional>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

namespace glance::power_status {
namespace {

constexpr const char* kTag = "power";
// VUSB above this is USB plugged in: 5V reads about 2.5V at the pin; on
// battery VUSB only sees the diode's leakage (~17mV measured on V2.1).
constexpr int kUsbPresentMillivolts = 3000;
constexpr int kSamples = 8;

// The pin's voltage in mV, averaged, on ADC1 (12dB: up to about 3.1V).
// nullopt if the ADC wouldn't start.
std::optional<int> readMillivolts(int pin) {
    adc_unit_t unit;
    adc_channel_t channel;
    if (adc_oneshot_io_to_channel(pin, &unit, &channel) != ESP_OK) {
        return std::nullopt;
    }
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t unitConfig{};
    unitConfig.unit_id = unit;
    if (adc_oneshot_new_unit(&unitConfig, &adc) != ESP_OK) {
        return std::nullopt;
    }
    adc_oneshot_chan_cfg_t channelConfig{};
    channelConfig.atten = ADC_ATTEN_DB_12;
    channelConfig.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_oneshot_config_channel(adc, channel, &channelConfig);
    adc_cali_handle_t cali = nullptr;
    adc_cali_curve_fitting_config_t caliConfig{};
    caliConfig.unit_id = unit;
    caliConfig.chan = channel;
    caliConfig.atten = ADC_ATTEN_DB_12;
    caliConfig.bitwidth = ADC_BITWIDTH_DEFAULT;
    bool calibrated = adc_cali_create_scheme_curve_fitting(&caliConfig, &cali) == ESP_OK;

    int sum = 0, count = 0;
    for (int i = 0; i < kSamples; i++) {
        int raw = 0, mv = 0;
        if (adc_oneshot_read(adc, channel, &raw) != ESP_OK) {
            continue;
        }
        if (!calibrated || adc_cali_raw_to_voltage(cali, raw, &mv) != ESP_OK) {
            mv = raw * 3100 / 4095;  // roughly, without the chip's calibration
        }
        sum += mv;
        count++;
    }
    if (calibrated) {
        adc_cali_delete_scheme_curve_fitting(cali);
    }
    adc_oneshot_del_unit(adc);
    if (count == 0) {
        return std::nullopt;
    }
    return sum / count;
}

// CHG_STAT as a plain input: no pull either way (see board.hpp).
bool chargeLineHigh() {
    const auto pin = static_cast<gpio_num_t>(board::kChargeStatusPin);
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << board::kChargeStatusPin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    return gpio_get_level(pin) != 0;
}

}  // namespace

Details readDetails() {
    Details details;
    if (auto mv = readMillivolts(board::kUsbDetectPin)) {
        details.usbMillivolts = *mv * 2;  // R3/R4 halve it
    }
    if (auto mv = readMillivolts(board::kChargeStatusPin)) {
        details.chargeMillivolts = *mv;  // how clear the level is (before the pin goes back to digital)
    }
    details.chargeLineHigh = chargeLineHigh();
    if (details.usbMillivolts < 0) {
        details.power = wake_log::Power::unknown;
    } else if (details.usbMillivolts < kUsbPresentMillivolts) {
        details.power = wake_log::Power::battery;
    } else {
        details.power = details.chargeLineHigh ? wake_log::Power::usb : wake_log::Power::charging;
    }
    return details;
}

wake_log::Power read() {
    Details details = readDetails();
    static constexpr const char* kNames[] = {"unknown", "battery", "USB, charging", "USB, not charging"};
    ESP_LOGI(kTag, "%s (VUSB %d mV)", kNames[static_cast<int>(details.power)], details.usbMillivolts);
    return details.power;
}

}  // namespace glance::power_status

#else  // V2.1: nothing to read

namespace glance::power_status {

wake_log::Power read() { return wake_log::Power::unknown; }
Details readDetails() { return {}; }

}  // namespace glance::power_status

#endif
