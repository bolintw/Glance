#include <ctime>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "calendar_fetch.hpp"
#include "calendar_view.hpp"
#include "canvas.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ics_event_collector.hpp"
#include "time_sync.hpp"
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

// The layout shows two columns of five (same as the Raspberry Pi version).
constexpr size_t kMaxEvents = 10;

// Empty slots are skipped. See components/calendar_fetch/Kconfig.projbuild.
constexpr const char* kIcsUrls[] = {
    CONFIG_GLANCE_ICS_URL_1, CONFIG_GLANCE_ICS_URL_2, CONFIG_GLANCE_ICS_URL_3,
    CONFIG_GLANCE_ICS_URL_4, CONFIG_GLANCE_ICS_URL_5,
};

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
std::optional<std::vector<ics::Occurrence>> fetchUpcomingEvents() {
    ics::EventCollector collector({
        .now = time(nullptr),
        .displayUtcOffset = time_sync::kUtcOffsetSeconds,
        .maxResults = kMaxEvents,
    });

    size_t lowestBefore = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t configured = 0;
    size_t fetched = 0;
    for (size_t slot = 0; slot < std::size(kIcsUrls); slot++) {
        const char* url = kIcsUrls[slot];
        if (url[0] == '\0') {
            continue;
        }
        configured++;
        esp_err_t err = calendar_fetch::fetchLines(url, [&](std::string_view line) { collector.onLine(line); });
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

    if (!display.init()) {
        return false;
    }
    // No clear() first: a full refresh already drives every pixel through
    // the whole waveform, so clearing only doubled the time and flicker.
    bool shown = display.flush(framebuffer);
    display.sleep();
    return shown;
}
}  // namespace

// One refresh: WiFi -> NTP -> calendars -> render -> panel. If any step
// before rendering fails, the panel is left alone: e-ink keeps showing the
// last good calendar, which beats replacing it with a wrong date or an
// empty list.
extern "C" void app_main(void)
{
    WifiManager wifi;
    esp_err_t err = wifi.connect(kWifiConnectTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi failed (%s), leaving the screen as is", esp_err_to_name(err));
        return;
    }
    // TLS certificate checks and "today" both need a real clock.
    err = time_sync::sync(kNtpSyncTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NTP sync failed (%s), leaving the screen as is", esp_err_to_name(err));
        return;
    }
    auto events = fetchUpcomingEvents();
    if (!events) {
        ESP_LOGE(kTag, "no calendar could be fetched, leaving the screen as is");
        return;
    }

    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    calendar_view::render(canvas, time(nullptr), time_sync::kUtcOffsetSeconds, *events);
    ESP_LOGI(kTag, "showing calendar");
    if (!show(framebuffer)) {
        ESP_LOGE(kTag, "panel did not respond, calendar not shown");
        return;
    }
    ESP_LOGI(kTag, "calendar shown");
}
