#include "battery.hpp"

#include <cstdint>
#include <ctime>
#include <vector>

#include "app_state.hpp"
#include "board.hpp"
#include "calendar_view.hpp"
#include "canvas.hpp"
#include "esp_log.h"
#include "fuel_gauge.hpp"
#include "peripherals.hpp"
#include "run_policy.hpp"

namespace glance {
namespace {

constexpr int64_t kCriticalCheckSeconds = 3600;

[[noreturn]] void waitForCharge() {
    if (!gBoot.batteryCritical) {
        ESP_LOGW(kTag, "battery critical: charge reminder up, checking hourly");
        gBoot.batteryCritical = true;
        gBoot.offlineShown = false;
        std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
        Canvas canvas(framebuffer, board::kPanelSize);
        calendar_view::renderChargeReminder(canvas);
        show(framebuffer);
        logRun(wake_log::Result::chargeReminder);
    } else {
        ESP_LOGI(kTag, "battery still too low, back to sleep");
        logRun(wake_log::Result::stillLow);
    }
    peripheralsOff();
    deepSleep(time(nullptr) + kCriticalCheckSeconds);  // even in development builds: it's on battery
}

}  // namespace

void checkBattery() {
    auto battery = fuel_gauge::read();
    if (battery && gBoot.fakeBatteryPct >= 0) {
        ESP_LOGW(kTag, "development: battery taken as %d%% (really %.1f%%)", gBoot.fakeBatteryPct,
                 battery->soc256 / 256.0);
        battery->soc256 = static_cast<uint16_t>(gBoot.fakeBatteryPct * 256);
    }
    gBoot.fakeBatteryPct = -1;  // one boot only
    if (!battery) {
        return;
    }
    gRun.millivolts = battery->millivolts;
    gRun.soc256 = battery->soc256;
    gRun.crate = battery->crate;
    ESP_LOGI(kTag, "battery %u mV, %.1f%%, %+.1f%%/h", battery->millivolts, battery->soc256 / 256.0,
             battery->crate * 0.208);
    if (run_policy::batteryTooLow(battery->soc256, battery->millivolts, gBoot.batteryCritical)) {
        waitForCharge();
    }
    if (gBoot.batteryCritical) {
        ESP_LOGI(kTag, "battery charged enough, back to normal");
        gBoot.batteryCritical = false;
    }
    gLowBattery = run_policy::batteryLow(battery->soc256);
}

}  // namespace glance
