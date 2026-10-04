#include <ctime>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "button.hpp"
#include "calendar_fetch.hpp"
#include "calendar_view.hpp"
#include "canvas.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ics_event_collector.hpp"
#include "refresh_schedule.hpp"
#include "settings.hpp"
#include "setup_mode.hpp"
#include "setup_view.hpp"
#include "time_sync.hpp"
#include "weather_fetch.hpp"
#include "wifi_manager.hpp"

#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
#include "serial_dump_display.hpp"
#else
#include "driver/spi_master.h"
#include "epd_7in5_v2.hpp"
#include "gpio.hpp"
#include "spi.hpp"
#endif

namespace {
constexpr const char* kTag = "main";
constexpr FrameSize kPanelSize{.width = 800, .height = 480};
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kNtpSyncTimeoutMs = 10000;
// A wake-up this close before the daily refresh time counts as that
// refresh, so waking slightly early doesn't redraw twice in one morning.
constexpr int64_t kMinRefreshGapSeconds = 3600;

// IO1: short press refreshes now, long press enters setup mode.
constexpr int kButtonPin = 1;
constexpr uint32_t kLongPressMs = 3000;
// Setup mode gives up (and goes back to the calendar) after this long
// without anyone loading its page.
constexpr uint32_t kSetupIdleTimeoutMs = 10 * 60 * 1000;
// Consecutive WiFi failures (an hour apart) before the screen says so. A
// router rebooting at 07:00 shouldn't wipe the calendar off the screen.
constexpr uint8_t kWifiFailuresBeforeNotice = 3;

// What one boot tells the next. Kept in RTC memory, which survives restarts
// and deep sleep but not power loss -- the magic tells a valid state from
// power-on garbage.
struct BootState {
    static constexpr uint32_t kMagic = 0x474C4E43;  // "GLNC"
    uint32_t magic;
    bool enterSetup;      // a long press asked for setup mode
    bool justConfigured;  // setup mode just saved; report WiFi trouble at once
    uint8_t wifiFailures;
};
RTC_NOINIT_ATTR BootState gBoot;

[[noreturn]] void restartInto(bool enterSetup) {
    gBoot.enterSetup = enterSetup;
    esp_restart();
}

// The layout shows two columns of five (same as the Raspberry Pi version).
constexpr size_t kMaxEvents = 10;

#ifndef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
// GPIO14: switches the V33_2 rail that powers the EPD, external flash, and
// LED. It's off by default to save power in deep sleep -- must be driven
// high before the EPD (or anything else on that rail) will respond to
// anything, including SPI commands and the busy pin.
constexpr int kPeripheralPowerPin = 14;

constexpr int kEpdMosiPin = 35;
constexpr int kEpdClkPin = 36;
constexpr int kEpdCsPin = 37;
constexpr int kEpdDcPin = 38;
constexpr int kEpdResetPin = 39;
constexpr int kEpdBusyPin = 40;
constexpr uint32_t kEpdSpiClockHz = 1'000'000;
#endif

// Fetches every configured calendar into one EventCollector. nullopt if
// calendars are configured but none could be fetched -- an empty list would
// wrongly read as "nothing coming up". Logs the slot number, never the URL,
// since the URL is a secret.
std::optional<std::vector<ics::Occurrence>> fetchUpcomingEvents(const Settings& settings) {
    ics::EventCollector collector({
        .now = time(nullptr),
        .displayUtcOffset = time_sync::kUtcOffsetSeconds,
        .maxResults = kMaxEvents,
    });

    size_t lowestBefore = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t configured = 0;
    size_t fetched = 0;
    for (size_t slot = 0; slot < settings.icsUrls.size(); slot++) {
        const std::string& url = settings.icsUrls[slot];
        if (url.empty()) {
            continue;
        }
        configured++;
        esp_err_t err =
            calendar_fetch::fetchLines(url.c_str(), [&](std::string_view line) { collector.onLine(line); });
        ESP_LOGI(kTag, "calendar %zu: %s", slot + 1, esp_err_to_name(err));
        fetched += err == ESP_OK;
    }
    ESP_LOGI(kTag, "heap lowest since boot: %zu before fetching, %zu after", lowestBefore,
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    if (configured > 0 && fetched == 0) {
        return std::nullopt;
    }

    auto events = collector.takeResults();
    ESP_LOGI(kTag, "next %zu events:", events.size());
    for (const auto& e : events) {
        time_t start = static_cast<time_t>(e.start);
        struct tm local;
        localtime_r(&start, &local);  // TZ was set by time_sync
        char when[24];
        strftime(when, sizeof(when), e.allDay ? "%m/%d (all day)" : "%m/%d %H:%M", &local);
        ESP_LOGI(kTag, "  %-16s %s", when, e.summary.empty() ? "(no title)" : e.summary.c_str());
    }
    return events;
}

// Brings up the panel (or its simulator stand-in), shows one frame and puts
// it back to sleep. The code below the backend selection is identical for
// both -- that's the point of the Display interface. False if the panel
// didn't respond.
bool show(std::span<const uint8_t> framebuffer) {
#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    SerialDumpDisplay display(kPanelSize);
#else
    Gpio peripheralPower(kPeripheralPowerPin, Gpio::Direction::output);
    peripheralPower.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    SpiConfig spiConfig{
        .mosiPin = kEpdMosiPin,
        .misoPin = -1,
        .sclkPin = kEpdClkPin,
        .csPin = kEpdCsPin,
        .hostId = SPI2_HOST,
        .clockSpeedHz = kEpdSpiClockHz,
        .maxTransferSize = kPanelSize.framebufferSize(),
    };
    Spi spi(spiConfig);

    EpdConfig epdConfig{
        .frame = kPanelSize,
        .spiDevice = spi,
        .dcPin = kEpdDcPin,
        .resetPin = kEpdResetPin,
        .busyPin = kEpdBusyPin,
    };
    Epd7in5V2 display(epdConfig);
#endif

    // No clear() first: a full refresh already drives every pixel through
    // the whole waveform, so clearing only doubled the time and flicker.
    bool ready = display.init();
    bool shown = ready && display.flush(framebuffer);
    if (ready) {
        display.sleep();
    }
#ifndef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    // Off again before deep sleep: with the external flash powered, its SPI
    // lines leak ~300uA; with the rail cut the whole board sleeps at ~70uA.
    peripheralPower.write(false);
#endif
    return shown;
}

void showNotice(std::string_view title, std::span<const std::string_view> lines) {
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    setup_view::renderNotice(canvas, title, lines);
    show(framebuffer);
}

// Setup mode: access point + setup page, QR code on the panel. Ends in a
// restart either way -- after a save, or when nobody used it -- so the next
// boot starts clean and draws the calendar.
[[noreturn]] void runSetupMode(const Settings& current) {
    ESP_LOGI(kTag, "entering setup mode");
    setup_mode::AccessPoint ap;
    if (setup_mode::start(current, ap) != ESP_OK) {
        restartInto(false);
    }
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    setup_view::render(canvas, ap.qrPayload, ap.ssid, ap.password, ap.url);
    show(framebuffer);

    gBoot.justConfigured = setup_mode::waitForSave(kSetupIdleTimeoutMs);
    ESP_LOGI(kTag, "leaving setup mode (%s)", gBoot.justConfigured ? "saved" : "idle");
    restartInto(false);
}

// Called after a WiFi failure: says so on the panel right after setup (most
// likely a mistyped password) or once failures keep piling up.
void reportWifiFailure() {
    gBoot.wifiFailures++;
    if (gBoot.justConfigured || gBoot.wifiFailures == kWifiFailuresBeforeNotice) {
        const std::string_view lines[] = {"每小時會自動重試", "長按按鈕可以重新設定"};
        showNotice("WiFi 連線失敗", lines);
    }
}

// One refresh: WiFi -> NTP -> calendars + weather -> render -> panel. If any
// step before rendering fails, the panel is left alone: e-ink keeps showing
// the last good calendar, which beats replacing it with a wrong date or an
// empty list. Weather is the exception -- without it the calendar is still
// worth showing, just with the weather spot left blank. True once the new
// calendar is on the panel.
bool refresh(const Settings& settings) {
    WifiManager wifi;
    esp_err_t err = wifi.connect(settings.wifiSsid, settings.wifiPassword, kWifiConnectTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi failed (%s), leaving the screen as is", esp_err_to_name(err));
        reportWifiFailure();
        return false;
    }
    gBoot.wifiFailures = 0;
    // TLS certificate checks and "today" both need a real clock.
    err = time_sync::sync(kNtpSyncTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NTP sync failed (%s), leaving the screen as is", esp_err_to_name(err));
        return false;
    }
    auto events = fetchUpcomingEvents(settings);
    if (!events) {
        ESP_LOGE(kTag, "no calendar could be fetched, leaving the screen as is");
        return false;
    }
    auto forecast = weather_fetch::fetchForecast(time(nullptr), settings.cwaApiKey, settings.weatherLocation);

    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    calendar_view::render(canvas, time(nullptr), time_sync::kUtcOffsetSeconds, *events, forecast);
    ESP_LOGI(kTag, "showing calendar");
    if (!show(framebuffer)) {
        ESP_LOGE(kTag, "panel did not respond, calendar not shown");
        return false;
    }
    ESP_LOGI(kTag, "calendar shown");
    return true;
}
}  // namespace

// Refresh once, then sleep until the next daily refresh -- or retry sooner
// if this one failed. A failed refresh may not even have a synced clock, so
// its retry is a plain delay rather than a time of day. Setup mode instead,
// if a long press asked for it or there's no WiFi to connect to.
extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "woke by %s", (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER)) ? "timer" : "reset/power-on");
    if (gBoot.magic != BootState::kMagic) {
        gBoot = {.magic = BootState::kMagic, .enterSetup = false, .justConfigured = false, .wifiFailures = 0};
    }
    ESP_ERROR_CHECK(settings::initStorage());
    Settings settings = settings::load();
    if (gBoot.enterSetup || settings.wifiSsid.empty()) {
        gBoot.enterSetup = false;
        runSetupMode(settings);
    }

    bool refreshed = refresh(settings);
    gBoot.justConfigured = false;
    int64_t now = time(nullptr);
    int64_t wakeAt = refreshed ? refresh_schedule::nextDaily(now, time_sync::kUtcOffsetSeconds,
                                                             CONFIG_GLANCE_REFRESH_HOUR, 0, kMinRefreshGapSeconds)
                               : now + CONFIG_GLANCE_RETRY_MINUTES * 60;
    int64_t sleepSeconds = wakeAt - now;

    time_t wakeTime = static_cast<time_t>(wakeAt);
    struct tm local;
    localtime_r(&wakeTime, &local);
    char when[24];
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
    ESP_LOGI(kTag, "next %s at %s (in %lldh%02lldm)", refreshed ? "refresh" : "retry", when, sleepSeconds / 3600,
             sleepSeconds % 3600 / 60);
#if CONFIG_GLANCE_DEEP_SLEEP
    // Give the USB serial port a moment to send the last log lines; deep
    // sleep cuts it off mid-buffer otherwise.
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep(static_cast<uint64_t>(sleepSeconds) * 1000000);
#else
    // Development mode: stay awake (USB serial stays reachable for flashing
    // and logs) and reboot at the same time deep sleep would have woken, or
    // when the button is pressed.
    // TODO(M9): wake from deep sleep on the button too.
    ESP_LOGW(kTag, "deep sleep disabled (menuconfig -> Glance Refresh), waiting awake instead");
    Button button(kButtonPin);
    while (time(nullptr) < wakeAt) {
        if (button.isDown()) {
            Button::Press press = button.readPress(kLongPressMs);
            if (press != Button::Press::none) {
                ESP_LOGI(kTag, "button: %s press", press == Button::Press::longPress ? "long" : "short");
                restartInto(press == Button::Press::longPress);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    restartInto(false);
#endif
}
