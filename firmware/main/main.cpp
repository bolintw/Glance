// One run of the calendar: wake up, check the battery, refresh the panel (or
// go into setup mode), then sleep until the next refresh. The parts:
//   app_state        what boots and the parts share (RTC boot state, wake log entry)
//   board.hpp        the pins
//   battery          the fuel gauge, the charge reminder
//   buttons          what a press means
//   refresh          calendar, privacy mode, the offline screen
//   setup_flow       setup mode's two steps
//   peripherals      the panel, the status LED and their power rail
//   serial_commands  development builds only

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app_state.hpp"
#include "battery.hpp"
#include "board.hpp"
#include "buttons.hpp"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_update.hpp"
#include "peripherals.hpp"
#include "refresh.hpp"
#include "run_policy.hpp"
#include "serial_commands.hpp"
#include "settings.hpp"
#include "setup_flow.hpp"
#include "setup_mode.hpp"
#include "time_sync.hpp"

namespace glance {
namespace {

// A wake-up this close before a daily refresh time counts as that refresh,
// so waking slightly early doesn't redraw twice in one morning. The 32 kHz
// crystal keeps the clock within seconds a day, so a few minutes is plenty;
// any more and a refresh run by hand (or a setup save) shortly before a
// refresh time would skip it.
constexpr int64_t kMinRefreshGapSeconds = 10 * 60;
static_assert(kMinRefreshGapSeconds < Settings::kMinRefreshGapMinutes * 60);

// The daily refresh times (Settings::refreshMinutes and the optional
// second), set once the settings are loaded.
std::vector<int> gRefreshMinutes{CONFIG_GLANCE_REFRESH_HOUR * 60};

// The longest a run may take from wake-up to sleep. A normal one takes about
// 15s; every step failing in turn about two minutes; an update download adds
// some. Each step has its own timeout, but a bug or a server that trickles
// could still keep the device awake until the battery is flat.
constexpr uint32_t kRunDeadlineMs = 5 * 60 * 1000;

// When to wake next, counting this run in (see run_policy::nextWake).
run_policy::NextWake scheduleNext(bool refreshed) {
    gBoot.failedRuns = refreshed ? 0 : gBoot.failedRuns + 1;
    // A run that didn't go online can't stand in for a daily refresh due
    // within the hour: privacy mode switched on at 06:30 still wakes at 07:00.
    const int64_t minGap = gRun.result == wake_log::Result::privacyCached ? 0 : kMinRefreshGapSeconds;
    return run_policy::nextWake(gBoot.failedRuns, time(nullptr), time_sync::kUtcOffsetSeconds, gRefreshMinutes,
                                CONFIG_GLANCE_RETRY_MINUTES, minGap);
}

// Crashes or brownouts keep recurring (run_policy::backOffAfterBadResets):
// each reboots straight away, so sleep a retry interval before trying again
// rather than rebooting nonstop. Development builds only log it, to keep
// the fault in reach of the serial console.
void backOffIfResetsKeepRecurring() {
    gBoot.badResets =
        run_policy::classifyReset(gRun.resetReason) == run_policy::ResetKind::normal ? 0 : gBoot.badResets + 1;
    if (!run_policy::backOffAfterBadResets(gBoot.badResets)) {
        return;
    }
    ESP_LOGE(kTag, "%u crashes or brownouts in a row", gBoot.badResets);
#if CONFIG_GLANCE_DEEP_SLEEP
    logRun(wake_log::Result::badResetBackoff);
    peripheralsOff();
    deepSleep(time(nullptr) + CONFIG_GLANCE_RETRY_MINUTES * 60);
#endif
}

// Cuts a run short once it's past kRunDeadlineMs: logs it and sleeps until
// the next attempt (it counts as a failed run), or restarts in development
// builds. A panel refresh in progress is let finish first (closePanel), so
// the panel isn't left half drawn. Setup mode and the development wait have
// their own limits, so a run that has moved on to those is left alone. A
// FreeRTOS task, above app_main's priority so a busy loop there can't hold
// it off.
void runDeadlineTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(kRunDeadlineMs));
    if (gPhase != Phase::refreshing) {
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGE(kTag, "run still going after %lu s: stuck, giving up", static_cast<unsigned long>(kRunDeadlineMs / 1000));
    if (!closePanel(kPanelFinishWaitMs)) {
        ESP_LOGE(kTag, "panel refresh still going, cutting it");
    }
    peripheralsOff();
    logRun(wake_log::Result::stuck);
    run_policy::NextWake next = scheduleNext(false);
#if CONFIG_GLANCE_DEEP_SLEEP
    deepSleep(next.at);
#else
    (void)next;
    restartInto(false);
#endif
}

// Why this run started, for the log and the wake log.
void noteWakeCause() {
    uint32_t causes = esp_sleep_get_wakeup_causes();
    gRun.resetReason = static_cast<uint8_t>(esp_reset_reason());
    gRun.wake = (causes & BIT(ESP_SLEEP_WAKEUP_TIMER))  ? wake_log::Wake::timer
                : (causes & (BIT(ESP_SLEEP_WAKEUP_EXT1) | BIT(ESP_SLEEP_WAKEUP_EXT0))) ? wake_log::Wake::button
                : esp_reset_reason() == ESP_RST_POWERON ? wake_log::Wake::powerOn
                                                        : wake_log::Wake::other;
    if (gRun.wake == wake_log::Wake::button) {
        rtc_gpio_deinit(static_cast<gpio_num_t>(board::kButtonPin));  // the wake-up left it an RTC pin
    }
    static constexpr const char* kWakeNames[] = {"power-on", "timer", "button", "reset"};
    ESP_LOGI(kTag, "firmware %s, woke by %s", ota_update::runningVersion(), kWakeNames[static_cast<int>(gRun.wake)]);
}

// After a refresh: sleep until the next one (see scheduleNext).
[[noreturn]] void sleepUntilNext(bool refreshed, const Settings& settings) {
    run_policy::NextWake next = scheduleNext(refreshed);
    const bool retry = next.retry;
    const int64_t wakeAt = next.at;
    int64_t sleepSeconds = wakeAt - time(nullptr);

    time_t wakeTime = static_cast<time_t>(wakeAt);
    struct tm local;
    localtime_r(&wakeTime, &local);
    char when[24];
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
    ESP_LOGI(kTag, "next %s at %s (in %lldh%02lldm)", retry ? "retry" : "refresh", when, sleepSeconds / 3600,
             sleepSeconds % 3600 / 60);
#if CONFIG_GLANCE_DEEP_SLEEP
    (void)settings;
    deepSleep(wakeAt);
#else
    // Development mode: stay awake (USB serial stays reachable for flashing
    // and logs) and reboot at the same time deep sleep would have woken. The
    // button task handles presses meanwhile.
    ESP_LOGW(kTag, "deep sleep disabled (menuconfig -> Glance Refresh), waiting awake instead");
    gPhase = Phase::waiting;
#if CONFIG_GLANCE_DEV_SETUP_ON_LAN
    // A run that didn't go online (privacy mode on a press) never brought the
    // network stack up: starting the web server on it hangs.
    if (gRun.result != wake_log::Result::privacyCached && setup_mode::startOnLan(settings) == ESP_OK) {
        ESP_LOGW(kTag, "development: setup pages served on the home network");
    }
#else
    (void)settings;
#endif
    while (time(nullptr) < wakeAt) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (setup_mode::wasSaved()) {
            vTaskDelay(pdMS_TO_TICKS(1500));  // let the "saved" page reach the browser
            if (setup_mode::eraseAsked()) {
                restartToErase();
            }
            break;
        }
    }
    restartInto(false);
#endif
}

}  // namespace
}  // namespace glance

// Setup mode instead of a refresh if a long press asked for it or there's
// no WiFi to connect to.
extern "C" void app_main(void)
{
    using namespace glance;
    time_sync::useLocalTimezone();
    noteWakeCause();
    initBootState();
    xTaskCreate(runDeadlineTask, "run_deadline", 4096, nullptr, 10, nullptr);
    ESP_ERROR_CHECK(settings::initStorage());
    if (std::exchange(gBoot.eraseAll, false)) {
        eraseAndShip();
    }
    backOffIfResetsKeepRecurring();
    checkBattery();  // doesn't return while the battery is too low to run
    if (std::exchange(gBoot.buttonStuck, false) && gRun.wake == wake_log::Wake::button) {
        // A stuck button came up (see deepSleep): not a press.
        ESP_LOGI(kTag, "button no longer stuck: back to sleep");
        logRun(wake_log::Result::backToSleep);
        deepSleep(gBoot.wakeAt != 0 ? std::optional<int64_t>(gBoot.wakeAt) : std::nullopt);
    }

    Settings settings = settings::load();
    gRefreshMinutes = {settings.refreshMinutes};
    if (settings.secondRefreshMinutes != Settings::kNoRefresh) {
        gRefreshMinutes.push_back(settings.secondRefreshMinutes);
    }
    bool noCalendars = std::ranges::all_of(settings.icsUrls, [](const std::string& url) { return url.empty(); });
    if (gRun.wake == wake_log::Wake::button) {
        actOnWakePress(noCalendars);
    }
    xTaskCreate(buttonTask, "button", 3072, nullptr, 5, nullptr);  // after NVS: a press writes to it
#if CONFIG_GLANCE_DEV_SERIAL_COMMANDS
    xTaskCreate(serialCommandTask, "serial_cmd", 4096, nullptr, 5, nullptr);
#endif
    // Setup mode on a long press or with no WiFi saved -- and right after the
    // WiFi step while there are no calendars yet: a first setup goes on to
    // the home-network step. (Saving that with no calendars is fine: the
    // device is then a photo frame.)
    bool homeStepNext = std::exchange(gBoot.homeStepNext, false);
    if (gBoot.enterSetup || settings.wifiSsid.empty() || (homeStepNext && noCalendars)) {
        gBoot.enterSetup = false;
        runSetupMode(settings);
    }

    gPhotoFrame = noCalendars;
    if (noCalendars) {
        ESP_LOGI(kTag, "no calendars: photo frame");
    }
    bool refreshed = noCalendars || settings::loadPrivacyMode() ? refreshPrivate(settings) : refresh(settings);
    gBoot.justConfigured = false;
    gBoot.pressed = false;
    peripheralsOff();
    logRun(gRun.result);
    sleepUntilNext(refreshed, settings);
}
