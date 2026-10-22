#include "serial_commands.hpp"

#include "sdkconfig.h"

#if CONFIG_GLANCE_DEV_SERIAL_COMMANDS

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <unistd.h>

#include "app_state.hpp"
#include "buttons.hpp"
#include "board.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "fuel_gauge.hpp"
#include "ota_update.hpp"
#include "freertos/task.h"
#include "peripherals.hpp"
#include "power_status.hpp"
#include "settings.hpp"
#include "soc/rtc.h"
#include "time_sync.hpp"
#include "wake_log_store.hpp"

namespace glance {
namespace {

// `glance hw`: what a new board needs checked (refs/private, V3.0 change
// list, section 9), one "hw:" line each so a script can pick them out.
void hardwareCheck() {
    std::printf("hw: board %s, firmware %s\n", board::kName, ota_update::runningVersion());
    const bool crystal = rtc_clk_slow_src_get() == SOC_RTC_SLOW_CLK_SRC_XTAL32K;
    std::printf("hw: slow clock %s, %lu Hz\n", crystal ? "32.768kHz crystal" : "internal RC (crystal not running?)",
                static_cast<unsigned long>(rtc_clk_slow_freq_get_hz()));
    power_status::Details power = power_status::readDetails();
    if (power.usbMillivolts < 0) {
        std::printf("hw: power status: no reading (ADC didn't start)\n");
    } else {
        static constexpr const char* kNames[] = {"unknown", "battery", "USB, charging", "USB, not charging"};
        std::printf("hw: VUSB %d mV, CHG_STAT %s (%d mV at the pin): %s\n", power.usbMillivolts,
                    power.chargeLineHigh ? "high" : "low", power.chargeMillivolts,
                    kNames[static_cast<int>(power.power)]);
    }
    if (auto battery = fuel_gauge::read()) {
        std::printf("hw: fuel gauge %u mV, %.1f%%, %+.1f%%/h, %s%s\n", battery->millivolts, battery->soc256 / 256.0,
                    battery->crate * 0.208, battery->hibernating ? "hibernating" : "active (sampling every 250ms)",
                    battery->alertWasSet ? ", alert was set" : "");
    } else {
        std::printf("hw: fuel gauge not answering\n");
    }
    std::printf("hw: panel %s\n", panelResponds() ? "answers" : "not answering (or busy drawing)");
    std::fflush(stdout);
}

}  // namespace

// Polls stdin: without the USB-Serial-JTAG driver installed, reads don't
// block, and installing it would change how the console output flows.
void serialCommandTask(void*) {
    std::string line;
    char chunk[32];
    while (true) {
        ssize_t n = read(fileno(stdin), chunk, sizeof(chunk));
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        for (ssize_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c != '\n' && c != '\r') {
                if (line.size() < 64) {
                    line += c;
                }
                continue;
            }
            if (line.empty()) {
                continue;
            }
            ESP_LOGW(kTag, "serial command: %s", line.c_str());
            if (line == "glance press short" || line == "glance press long") {
                actOnPress(line.ends_with("long"), [] { return Hold::released; });
            } else if (line == "glance privacy on" || line == "glance privacy off") {
                settings::savePrivacyMode(line.ends_with("on"));
                restartInto(false);
            } else if (line == "glance refresh") {
                restartInto(false);
            } else if (line == "glance hw") {
                hardwareCheck();
            } else if (line == "glance log") {
                std::fputs(wake_log::csvHeader().c_str(), stdout);
                wake_log_store::forEach([](const wake_log::Entry& e) {
                    std::fputs(wake_log::csvLine(e, time_sync::kUtcOffsetSeconds).c_str(), stdout);
                });
                std::fflush(stdout);
            } else if (line.starts_with("glance battery ") && line.size() > 15) {
                gBoot.fakeBatteryPct = static_cast<int8_t>(std::clamp(std::atoi(line.c_str() + 15), 0, 100));
                ESP_LOGW(kTag, "next boot sees the battery at %d%%", gBoot.fakeBatteryPct);
                restartInto(false);
            } else if (line == "glance sleep" || line == "glance sleep ext0") {
                bool ext0 = line.ends_with("ext0");
                ESP_LOGW(kTag, "deep sleep until the button (%s)", ext0 ? "ext0, RTC peripherals on" : "ext1");
                peripheralsOff();
                deepSleep(std::nullopt, ext0);
            } else if (line == "glance refresh configured") {
                gBoot.justConfigured = true;
                restartInto(false);
            } else if (line == "glance wifi default") {
                if (settings::forgetWifi() == ESP_OK) {
                    ESP_LOGW(kTag, "saved WiFi forgotten, using menuconfig's; restarting");
                    restartInto(false);
                }
            } else if (line.starts_with("glance wifi ") && line.size() > 12) {
                Settings current = settings::load();
                current.wifiSsid = line.substr(12);
                if (settings::save(current) == ESP_OK) {
                    ESP_LOGW(kTag, "WiFi network set, restarting");
                    restartInto(false);
                }
            } else {
                ESP_LOGW(kTag, "unknown command");
            }
            line.clear();
        }
    }
}

}  // namespace glance

#endif  // CONFIG_GLANCE_DEV_SERIAL_COMMANDS
