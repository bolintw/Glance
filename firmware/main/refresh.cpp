#include "refresh.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sys/time.h>
#include <vector>

#include "app_state.hpp"
#include "board.hpp"
#include "calendar_fetch.hpp"
#include "calendar_view.hpp"
#include "canvas.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "ics_event_collector.hpp"
#include "ota_update.hpp"
#include "peripherals.hpp"
#include "photo_store.hpp"
#include "photos.hpp"
#include "run_policy.hpp"
#include "settings.hpp"
#include "time_sync.hpp"
#include "weather_fetch.hpp"
#include "wifi_manager.hpp"

namespace glance {
namespace {

constexpr uint32_t kNtpSyncTimeoutMs = 10000;
// The layout shows two columns of five (same as the Raspberry Pi version).
constexpr size_t kMaxEvents = 10;

// Privacy mode's photo, both ways (1bpp, and 2bpp for 4-gray panels). When
// it's an upload, `mapping` keeps it readable from flash.
struct PrivacyPhoto {
    Bitmap mono;
    GrayBitmap gray;
    std::unique_ptr<photo_store::Photo> mapping;
};

// gBoot.lastPhoto holds an upload's slot, or this plus a built-in's index.
constexpr uint8_t kBuiltInPhotoKey = 0x40;

// The uploaded photos if there are any (a fresh upload first, then shuffled
// rounds -- see photo_rotation), otherwise the built-in ones, never the same
// one twice in a row.
PrivacyPhoto pickPhoto() {
    uint8_t key;
    std::vector<int> slots = photo_store::list();
    if (!slots.empty()) {
        photo_rotation::State rotation = settings::loadPhotoRotation();
        int last = gBoot.lastPhoto < kBuiltInPhotoKey ? gBoot.lastPhoto : -1;
        key = static_cast<uint8_t>(photo_rotation::pick(slots, rotation, last, [] { return esp_random(); }));
        settings::savePhotoRotation(rotation);
    } else {
        std::vector<uint8_t> keys;
        for (size_t i = 0; i < photos::kBuiltIn.size(); i++) {
            keys.push_back(static_cast<uint8_t>(kBuiltInPhotoKey + i));
        }
        if (keys.size() > 1) {
            std::erase(keys, gBoot.lastPhoto);
        }
        key = keys[esp_random() % keys.size()];
    }
    gBoot.lastPhoto = key;

    if (key < kBuiltInPhotoKey) {
        if (auto mapping = photo_store::open(key)) {
            ESP_LOGI(kTag, "privacy photo: uploaded, slot %d", key);
            Bitmap mono{mapping->width(), mapping->height(), mapping->mono()};
            GrayBitmap gray{mapping->width(), mapping->height(), mapping->gray()};
            return {mono, gray, std::move(mapping)};
        }
        key = kBuiltInPhotoKey;  // unreadable upload: fall back to a built-in
    }
    size_t index = key - kBuiltInPhotoKey;
    ESP_LOGI(kTag, "privacy photo: built-in %zu", index);
    return {photos::kBuiltIn[index], photos::kBuiltInGray[index], nullptr};
}

// The 4-gray overlay for a photo when the panel is driven in 4-gray mode.
std::optional<GrayOverlay> grayOverlayFor([[maybe_unused]] const PrivacyPhoto& photo) {
#if CONFIG_GLANCE_PHOTO_4GRAY
    return calendar_view::photoOverlay(photo.gray);
#else
    return std::nullopt;
#endif
}

// Fetches every configured calendar into one EventCollector. nullopt if
// calendars are configured but none could be fetched -- an empty list would
// wrongly read as "nothing coming up". Logs the slot number, never the URL,
// since the URL is a secret. Keeps track of when each calendar started
// failing; `stale` says whether one has failed long enough to show
// (run_policy::calendarStale).
std::optional<std::vector<ics::Occurrence>> fetchUpcomingEvents(const Settings& settings, bool& stale) {
    const int64_t now = time(nullptr);
    settings::CalendarFailures failures = settings::loadCalendarFailures();
    const settings::CalendarFailures before = failures;
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
            failures[slot] = 0;
            continue;
        }
        configured++;
        esp_err_t err =
            calendar_fetch::fetchLines(url.c_str(), [&](std::string_view line) { collector.onLine(line); });
        ESP_LOGI(kTag, "calendar %zu: %s", slot + 1, esp_err_to_name(err));
        fetched += err == ESP_OK;
        if (err == ESP_OK) {
            failures[slot] = 0;
        } else if (failures[slot] == 0) {
            failures[slot] = static_cast<uint32_t>(now);
        }
    }
    if (failures != before) {
        settings::saveCalendarFailures(failures);
    }
    stale = std::ranges::any_of(failures, [&](uint32_t since) { return run_policy::calendarStale(since, now); });
    if (stale) {
        ESP_LOGW(kTag, "a calendar hasn't come through for two days");
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
        localtime_r(&start, &local);  // TZ set at boot (time_sync::useLocalTimezone)
        char when[24];
        strftime(when, sizeof(when), e.allDay ? "%m/%d (all day)" : "%m/%d %H:%M", &local);
        ESP_LOGI(kTag, "  %-16s %s", when, e.summary.empty() ? "(no title)" : e.summary.c_str());
    }
    return events;
}

// Notes how long it took and the signal for the wake log.
void noteConnected(int64_t startedUs) {
    gRun.wifiMs = static_cast<uint16_t>(std::min<int64_t>((esp_timer_get_time() - startedUs) / 1000, 0xFFFE));
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        gRun.rssi = ap.rssi;
    }
}

// Connects to the saved WiFi and remembers it as working. Right after setup
// saved a network that doesn't connect, falls back to the last one that did
// and makes that the saved one again (`settings` follows); the setup page
// tells the user next time.
esp_err_t joinWifi(WifiManager& wifi, Settings& settings) {
    int64_t started = esp_timer_get_time();
    esp_err_t err = wifi.connect(settings.wifiSsid, settings.wifiPassword, kWifiConnectTimeoutMs);
    if (err == ESP_OK) {
        noteConnected(started);
        settings::rememberWorkingWifi({settings.wifiSsid, settings.wifiPassword});
        return ESP_OK;
    }
    auto working = settings::loadWorkingWifi();
    if (!gBoot.justConfigured || !working ||
        (working->ssid == settings.wifiSsid && working->password == settings.wifiPassword)) {
        return err;
    }
    ESP_LOGW(kTag, "the new WiFi didn't connect, trying the last one that worked");
    if (wifi.connect(working->ssid, working->password, kWifiConnectTimeoutMs) != ESP_OK) {
        return err;
    }
    noteConnected(started);
    ESP_LOGW(kTag, "back on the WiFi that worked");
    settings::revertWifi(settings.wifiSsid, *working);
    settings.wifiSsid = working->ssid;
    settings.wifiPassword = working->password;
    return ESP_OK;
}

// NTP, noting how far it moved a clock that was already running (the RTC
// across deep sleep): the 32.768kHz crystal's drift, for the wake log.
esp_err_t syncClock() {
    bool hadClock = time_sync::clockIsPlausible();
    struct timeval before;
    gettimeofday(&before, nullptr);
    int64_t startedUs = esp_timer_get_time();
    esp_err_t err = time_sync::sync(kNtpSyncTimeoutMs);
    if (err == ESP_OK && hadClock) {
        struct timeval after;
        gettimeofday(&after, nullptr);
        int64_t movedUs = (after.tv_sec - before.tv_sec) * 1000000LL + (after.tv_usec - before.tv_usec) -
                          (esp_timer_get_time() - startedUs);
        gRun.clockDriftMs = static_cast<int32_t>(std::clamp<int64_t>(movedUs / 1000, INT32_MIN + 1, INT32_MAX));
        ESP_LOGI(kTag, "NTP moved the clock by %ld ms", static_cast<long>(gRun.clockDriftMs));
    }
    return err;
}

// The date, if the clock is real (a restart keeps it, power loss doesn't).
std::optional<int64_t> plausibleNow() {
    if (time_sync::clockIsPlausible()) {
        return time(nullptr);
    }
    return std::nullopt;
}

// Privacy mode's layout plus a status icon, for when there's no calendar to
// show: offline (no weather, crossed-out WiFi), or no calendar for two days
// (weather, calendar with a cross).
void showWithoutCalendar(const std::optional<weather::Forecast>& forecast, calendar_view::Status status,
                         const char* what) {
    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    PrivacyPhoto photo = pickPhoto();
    status.lowBattery = gLowBattery;
    calendar_view::renderPrivate(canvas, plausibleNow(), time_sync::kUtcOffsetSeconds, forecast, photo.mono, status);
    auto overlay = grayOverlayFor(photo);
    ESP_LOGI(kTag, "showing the %s screen", what);
    if (show(framebuffer, overlay ? &*overlay : nullptr)) {
        gBoot.offlineShown = true;
    }
}

void showOffline() { showWithoutCalendar(std::nullopt, {.offline = true}, "offline"); }

// The calendar couldn't get online: the offline screen if it's due,
// otherwise the last calendar stays up.
void reportOffline() {
    gBoot.wifiFailures++;
    bool due = run_policy::offlineScreenDue(gBoot.justConfigured, gBoot.pressed, gBoot.wifiFailures);
    if (due && !gBoot.offlineShown) {
        showOffline();
    }
    gRun.result = gBoot.offlineShown ? wake_log::Result::offlineShown : wake_log::Result::offlineKept;
}

// Last thing a successful online refresh does: the screen is already up to
// date, so a download (only when there's a new release) costs nothing
// visible. Restarts into the update if one is installed.
void installUpdateIfAny() {
    ota_update::markRunningAppValid();
    if (gLowBattery) {
        ESP_LOGI(kTag, "battery low: no update check");
        return;
    }
    ota_update::checkAndInstall(std::string(CONFIG_GLANCE_OTA_RELEASE_URL) + "/" + board::kOtaManifest);
}

}  // namespace

bool refreshPrivate(Settings& settings) {
    std::optional<weather::Forecast> forecast;
    bool online = false;
    {
        WifiManager wifi;
        esp_err_t err = joinWifi(wifi, settings);
        if (err == ESP_OK) {
            err = syncClock();
            online = err == ESP_OK;
        }
        gBoot.wifiFailures = online ? 0 : gBoot.wifiFailures + 1;
        if (online) {
            forecast = weather_fetch::fetchForecast(time(nullptr), settings.cwaApiKey, settings.weatherLocation);
        } else {
            ESP_LOGE(kTag, "offline (%s), drawing privacy mode without weather", esp_err_to_name(err));
        }
    }

    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    PrivacyPhoto photo = pickPhoto();
    calendar_view::renderPrivate(canvas, plausibleNow(), time_sync::kUtcOffsetSeconds, forecast, photo.mono,
                                 {.offline = !online, .lowBattery = gLowBattery});
    auto overlay = grayOverlayFor(photo);
    ESP_LOGI(kTag, "showing privacy mode%s", overlay ? " (4-gray)" : "");
    if (!show(framebuffer, overlay ? &*overlay : nullptr)) {
        ESP_LOGE(kTag, "panel did not respond");
        gRun.result = wake_log::Result::panelFailed;
        return false;
    }
    gBoot.offlineShown = !online;
    gRun.result = online ? wake_log::Result::privacy : wake_log::Result::offlineShown;
    if (online) {
        installUpdateIfAny();
    }
    return online;
}

bool refresh(Settings& settings) {
    WifiManager wifi;
    esp_err_t err = joinWifi(wifi, settings);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "WiFi failed (%s)", esp_err_to_name(err));
        reportOffline();
        return false;
    }
    // TLS certificate checks and "today" both need a real clock. Without it
    // there's no internet to speak of: that's offline too.
    err = syncClock();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NTP sync failed (%s)", esp_err_to_name(err));
        reportOffline();
        return false;
    }
    gBoot.wifiFailures = 0;
    bool calendarStale = false;
    auto events = fetchUpcomingEvents(settings, calendarStale);
    if (!events && !calendarStale) {
        ESP_LOGE(kTag, "no calendar could be fetched, leaving the screen as is");
        gRun.result = wake_log::Result::fetchFailed;
        return false;
    }
    auto forecast = weather_fetch::fetchForecast(time(nullptr), settings.cwaApiKey, settings.weatherLocation);
    if (!events) {
        // Nothing to draw a calendar with, for two days now: say so rather
        // than leave an ever older calendar up.
        showWithoutCalendar(forecast, {.calendarError = true}, "no-calendar");
        gRun.result = wake_log::Result::calendarError;
        return false;
    }

    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    calendar_view::render(canvas, time(nullptr), time_sync::kUtcOffsetSeconds, *events, forecast,
                          {.lowBattery = gLowBattery, .calendarError = calendarStale});
    ESP_LOGI(kTag, "showing calendar");
    if (!show(framebuffer)) {
        ESP_LOGE(kTag, "panel did not respond, calendar not shown");
        gRun.result = wake_log::Result::panelFailed;
        return false;
    }
    ESP_LOGI(kTag, "calendar shown");
    gBoot.offlineShown = false;
    gRun.result = wake_log::Result::calendar;
    installUpdateIfAny();
    return true;
}

}  // namespace glance
