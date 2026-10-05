#include <algorithm>
#include <atomic>
#include <unistd.h>
#include <string>
#include <functional>
#include <ctime>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "button.hpp"
#include "calendar_fetch.hpp"
#include "calendar_view.hpp"
#include "canvas.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "ics_event_collector.hpp"
#include "ota_update.hpp"
#include "photo_store.hpp"
#include "photos.hpp"
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

// IO1: short press toggles privacy mode, long press enters setup mode.
constexpr int kButtonPin = 1;
constexpr uint32_t kLongPressMs = 3000;
// IO48: status LED, powered from the peripheral rail (GPIO14) and lit by
// driving IO48 low (per the schematic). Blinks on a short press; stays lit
// once a press has become a long press, so you know when to let go.
constexpr gpio_num_t kStatusLedPin = GPIO_NUM_48;
constexpr gpio_num_t kPeripheralRailPin = GPIO_NUM_14;
// Setup mode gives up (and goes back to the calendar) after this long
// without anyone loading its page.
constexpr uint32_t kSetupIdleTimeoutMs = 10 * 60 * 1000;
// Consecutive failures to get online (an hour apart) before the calendar
// gives way to the offline screen. A router rebooting at 07:00 shouldn't
// wipe the calendar off the screen.
constexpr uint8_t kOfflineRunsBeforeScreen = 3;

// What one boot tells the next. Kept in RTC memory, which survives restarts
// and deep sleep but not power loss -- the magic tells a valid state from
// power-on garbage.
struct BootState {
    static constexpr uint32_t kMagic = 0x474C4E34;  // "GLN4" (bump when the layout changes)
    uint32_t magic;
    bool enterSetup;        // a long press asked for setup mode
    bool accessPointSetup;  // ...and it has to be the WiFi step on the access point
    bool wifiFailed;        // ...because the saved WiFi didn't connect (say so on the form)
    bool justConfigured;    // setup mode just saved; report WiFi trouble at once
    bool homeStepNext;      // the access point's WiFi step just saved: a first setup goes on
    bool pressed;           // a short press asked for this run: the screen has to change
    bool offlineShown;      // the panel shows the offline icon: short presses don't switch modes
    uint8_t wifiFailures;   // runs in a row that couldn't get online
    uint8_t lastPhoto;  // privacy mode's last photo (see pickPhoto), so the next one differs
};
RTC_NOINIT_ATTR BootState gBoot;

[[noreturn]] void restartInto(bool enterSetup) {
    ota_update::markRunningAppValid();  // a deliberate restart isn't a failed update
    gBoot.enterSetup = enterSetup;
    esp_restart();
}

// What the device is doing, for the button task to decide what a press means.
enum class Phase { refreshing, waiting, setup };
std::atomic<Phase> gPhase{Phase::refreshing};
// No calendars configured: a photo frame, always privacy mode's layout.
std::atomic<bool> gPhotoFrame{false};

// Raw driver calls rather than Gpio: Gpio's constructor resets the pin,
// which would cut the rail under a panel refresh in progress.
void statusLedOn() {
    gpio_set_direction(kPeripheralRailPin, GPIO_MODE_OUTPUT);
    gpio_set_level(kPeripheralRailPin, 1);
    gpio_reset_pin(kStatusLedPin);
    gpio_set_direction(kStatusLedPin, GPIO_MODE_OUTPUT);
    gpio_set_level(kStatusLedPin, 0);  // active low
}

// End of a run: the rail and the LED off, whether or not the panel was drawn
// (show() cuts the rail itself, but a run that never got to draw left the
// LED lit by the press that started it).
void peripheralsOff() {
    gpio_set_direction(kStatusLedPin, GPIO_MODE_OUTPUT);
    gpio_set_level(kStatusLedPin, 0);  // parked low: nothing to feed back into the unpowered rail
    gpio_set_direction(kPeripheralRailPin, GPIO_MODE_OUTPUT);
    gpio_set_level(kPeripheralRailPin, 0);
}

// What a press does (from the button, or a serial command in development):
//   short: toggle privacy mode and redraw at once (cutting short any
//          refresh in progress: a guest at the door shouldn't wait for it)
//          -- except while the panel shows the offline icon, or with no
//          calendars at all (a photo frame): there's no calendar to switch
//          to, so it's ignored (no blink either: asleep, as the device
//          mostly will be, it couldn't blink anyway)
//   long:  setup mode
//   in setup mode, any press leaves it
// `waitForRelease` runs once a long press has lit the LED: setup mode starts
// when the button is let go.
void actOnPress(bool isLong, const std::function<void()>& waitForRelease) {
    ESP_LOGI(kTag, "button: %s press", isLong ? "long" : "short");
    if (gPhase == Phase::setup) {
        restartInto(false);
    }
    if (!isLong && (gBoot.offlineShown || gPhotoFrame)) {
        ESP_LOGI(kTag, "%s: short press ignored", gPhotoFrame ? "no calendars" : "offline");
        return;
    }
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

// Watches the button from boot on, so a press is timed from when it really
// started even while a refresh keeps app_main busy.
void buttonTask(void*) {
    Button button(kButtonPin);
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

#if CONFIG_GLANCE_DEV_SERIAL_COMMANDS
// Development aid: lines typed on the USB serial console that stand in for
// the button, so tests can drive it without anyone there.
//   glance press short | glance press long
//   glance privacy on | glance privacy off
//   glance refresh
//   glance refresh configured    (as if setup had just saved: WiFi fallback, offline screen at once)
//   glance wifi <network name>   (keeps the saved password)
//   glance wifi default          (back to the menuconfig network and password)
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
                actOnPress(line.ends_with("long"), [] {});
            } else if (line == "glance privacy on" || line == "glance privacy off") {
                settings::savePrivacyMode(line.ends_with("on"));
                restartInto(false);
            } else if (line == "glance refresh") {
                restartInto(false);
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
#endif

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
constexpr int kFlashCsPin = 21;  // external W25Q128, on the same rail and SPI lines
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

// init -> one frame -> sleep, on whichever backend. False if the panel
// didn't respond.
bool drawOn(Display& display, std::span<const uint8_t> framebuffer, const GrayOverlay* overlay) {
    // No clear() first: a full refresh already drives every pixel through
    // the whole waveform, so clearing only doubled the time and flicker.
    bool ready = display.init();
    bool shown = ready && (overlay ? display.flushGray(framebuffer, *overlay) : display.flush(framebuffer));
    if (ready) {
        display.sleep();
    }
    return shown;
}

// Brings up the panel (or its simulator stand-in), shows one frame and puts
// it back to sleep. The code below the backend selection is identical for
// both -- that's the point of the Display interface. False if the panel
// didn't respond.
bool show(std::span<const uint8_t> framebuffer, const GrayOverlay* overlay = nullptr) {
#ifdef CONFIG_GLANCE_DISPLAY_BACKEND_SIMULATOR
    SerialDumpDisplay display(kPanelSize);
    return drawOn(display, framebuffer, overlay);
#else
    Gpio peripheralPower(kPeripheralPowerPin, Gpio::Direction::output);
    peripheralPower.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    bool shown;
    {
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
        shown = drawOn(display, framebuffer, overlay);
    }  // SPI bus released

    // Park the lines into the rail's chips low before cutting it: a line left
    // high feeds the unpowered rail through the chips' protection diodes
    // (the status LED on that rail was seen glowing dimly with it "off").
    for (int pin : {kEpdMosiPin, kEpdClkPin, kEpdCsPin, kEpdDcPin, kEpdResetPin, kFlashCsPin}) {
        auto gpio = static_cast<gpio_num_t>(pin);
        gpio_reset_pin(gpio);
        gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(gpio, 0);
    }
    // Off again before deep sleep: with the external flash powered, its SPI
    // lines leak ~300uA; with the rail cut the whole board sleeps at ~70uA.
    peripheralPower.write(false);
    return shown;
#endif
}

void showNotice(std::string_view title, std::span<const std::string_view> lines) {
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    setup_view::renderNotice(canvas, title, lines);
    show(framebuffer);
}

// Either setup step's events, until it ends in a restart -- after a save,
// or when nobody used it -- so the next boot starts clean.
[[noreturn]] void runSetupEvents(bool onAccessPoint, const std::string& accessPointUrl) {
    gPhase = Phase::setup;
    while (true) {
        switch (setup_mode::waitForEvent(kSetupIdleTimeoutMs)) {
            case setup_mode::Event::saved:
                ESP_LOGI(kTag, "leaving setup mode (saved)");
                gBoot.justConfigured = true;
                gBoot.homeStepNext = onAccessPoint;
                restartInto(false);
            case setup_mode::Event::idle:
                ESP_LOGI(kTag, "leaving setup mode (idle)");
                restartInto(false);
            case setup_mode::Event::erased:
                ESP_LOGW(kTag, "everything erased, starting over");
                gBoot.magic = 0;  // forget this boot state too
                esp_restart();
            case setup_mode::Event::joined: {
                // The phone is in: take the QR code (and the access point's
                // password) off the screen so nobody else can use them.
                const std::string fallback = "沒有跳出來的話請開啟 " + accessPointUrl;
                if (onAccessPoint) {
                    const std::string_view lines[] = {"請在手機上完成設定", fallback};
                    showNotice("手機已連上", lines);
                } else {
                    const std::string_view lines[] = {"請在手機上完成設定"};
                    showNotice("手機已連上", lines);
                }
                break;
            }
            case setup_mode::Event::intruder: {
                // A second device has the secret: close up. Setting up again
                // makes a new password or token.
                setup_mode::close();
                const std::string_view lines[] = {onAccessPoint ? "已關閉熱點" : "已關閉設定頁", "請長按按鈕重新設定"};
                showNotice("偵測到第二台裝置連線", lines);
                break;  // stays here until a press, or the idle timeout
            }
        }
    }
}

// The WiFi step, on the access point: the QR code joins it.
[[noreturn]] void runAccessPointSetup(const Settings& current) {
    ESP_LOGI(kTag, "entering setup mode: WiFi step on the access point");
    std::string error;
    if (gBoot.wifiFailed) {
        error = "連不上「" + current.wifiSsid + "」，請確認網路名稱和密碼";
        gBoot.wifiFailed = false;
    }
    setup_mode::AccessPoint ap;
    if (setup_mode::start(current, ap, error) != ESP_OK) {
        restartInto(false);
    }
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    setup_view::render(canvas, ap.qrPayload, ap.ssid, ap.password, ap.url);
    show(framebuffer);
    runSetupEvents(true, ap.url);
}

// Calendars, weather and photos, on the home network: the phone keeps its
// internet to look up calendar URLs. Falls back to the WiFi step when the
// saved WiFi doesn't connect.
[[noreturn]] void runHomeSetup(const Settings& current) {
    ESP_LOGI(kTag, "entering setup mode on the home network");
    WifiManager wifi;  // stays up for the session
    if (wifi.connect(current.wifiSsid, current.wifiPassword, kWifiConnectTimeoutMs) != ESP_OK) {
        // The access point brings WiFi up its own way, so it needs a fresh boot.
        ESP_LOGW(kTag, "WiFi didn't connect, going to the WiFi step");
        gBoot.accessPointSetup = true;
        gBoot.wifiFailed = true;
        restartInto(true);
    }
    settings::rememberWorkingWifi({current.wifiSsid, current.wifiPassword});
    setup_mode::HomeNetwork home;
    if (setup_mode::startHome(current, home) != ESP_OK) {
        restartInto(false);
    }
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    setup_view::renderLan(canvas, home.qrPayload);
    show(framebuffer);
    runSetupEvents(false, {});
}

// Setup mode: on the home network when there's a WiFi to join, otherwise
// the WiFi step on the access point first.
[[noreturn]] void runSetupMode(const Settings& current) {
    if (gBoot.accessPointSetup || current.wifiSsid.empty()) {
        gBoot.accessPointSetup = false;
        runAccessPointSetup(current);
    }
    runHomeSetup(current);
}

// Connects to the saved WiFi and remembers it as working. Right after setup
// saved a network that doesn't connect, falls back to the last one that did
// and makes that the saved one again (`settings` follows); the setup page
// tells the user next time.
esp_err_t joinWifi(WifiManager& wifi, Settings& settings) {
    esp_err_t err = wifi.connect(settings.wifiSsid, settings.wifiPassword, kWifiConnectTimeoutMs);
    if (err == ESP_OK) {
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
    ESP_LOGW(kTag, "back on the WiFi that worked");
    settings::revertWifi(settings.wifiSsid, *working);
    settings.wifiSsid = working->ssid;
    settings.wifiPassword = working->password;
    return ESP_OK;
}

// Privacy mode's layout without weather, plus the crossed-out WiFi icon:
// what the panel shows when the device can't get online. The date only if
// the clock survived (a restart keeps it, power loss doesn't).
void showOffline() {
    std::optional<int64_t> now;
    if (time_sync::clockIsPlausible()) {
        now = time(nullptr);
    }
    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    PrivacyPhoto photo = pickPhoto();
    calendar_view::renderPrivate(canvas, now, time_sync::kUtcOffsetSeconds, std::nullopt, photo.mono, true);
#if CONFIG_GLANCE_PHOTO_4GRAY
    GrayOverlay overlay = calendar_view::photoOverlay(photo.gray);
    const GrayOverlay* gray = &overlay;
#else
    const GrayOverlay* gray = nullptr;
#endif
    ESP_LOGI(kTag, "showing the offline screen");
    if (show(framebuffer, gray)) {
        gBoot.offlineShown = true;
    }
}

// The calendar couldn't get online. Leaves the last calendar up unless
// that would hide a problem the user needs to see: right after setup (most
// likely a mistyped password), after a press (it has to change something),
// or once the failures pile up.
void reportOffline() {
    gBoot.wifiFailures++;
    bool due = gBoot.justConfigured || gBoot.pressed || gBoot.wifiFailures >= kOfflineRunsBeforeScreen;
    if (due && !gBoot.offlineShown) {
        showOffline();
    }
}

// Last thing a successful online refresh does: the screen is already up to
// date, so a download (only when there's a new release) costs nothing
// visible. Restarts into the update if one is installed.
void installUpdateIfAny() {
    ota_update::markRunningAppValid();
    ota_update::checkAndInstall(CONFIG_GLANCE_OTA_MANIFEST_URL);
}

// Privacy mode's refresh: date, weather and a photo, no calendars fetched at
// all. Unlike the calendar it always redraws, even without WiFi or a synced
// clock -- the point is that the events come off the screen. True if it
// got everything; false means retry later for the date and weather.
bool refreshPrivate(Settings& settings) {
    std::optional<weather::Forecast> forecast;
    bool online = false;
    {
        WifiManager wifi;
        esp_err_t err = joinWifi(wifi, settings);
        if (err == ESP_OK) {
            err = time_sync::sync(kNtpSyncTimeoutMs);
            online = err == ESP_OK;
        }
        gBoot.wifiFailures = online ? 0 : gBoot.wifiFailures + 1;
        if (online) {
            forecast = weather_fetch::fetchForecast(time(nullptr), settings.cwaApiKey, settings.weatherLocation);
        } else {
            ESP_LOGE(kTag, "offline (%s), drawing privacy mode without weather", esp_err_to_name(err));
        }
    }
    std::optional<int64_t> now;
    if (time_sync::clockIsPlausible()) {
        now = time(nullptr);
    }

    std::vector<uint8_t> framebuffer(kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, kPanelSize);
    PrivacyPhoto photo = pickPhoto();
    calendar_view::renderPrivate(canvas, now, time_sync::kUtcOffsetSeconds, forecast, photo.mono, !online);
#if CONFIG_GLANCE_PHOTO_4GRAY
    GrayOverlay overlay = calendar_view::photoOverlay(photo.gray);
    const GrayOverlay* gray = &overlay;
#else
    const GrayOverlay* gray = nullptr;
#endif
    ESP_LOGI(kTag, "showing privacy mode%s", gray ? " (4-gray)" : "");
    if (!show(framebuffer, gray)) {
        ESP_LOGE(kTag, "panel did not respond");
        return false;
    }
    gBoot.offlineShown = !online;
    if (online) {
        installUpdateIfAny();
    }
    return online;
}

// One refresh: WiFi -> NTP -> calendars + weather -> render -> panel. If any
// step before rendering fails, the panel is left alone: e-ink keeps showing
// the last good calendar, which beats replacing it with a wrong date or an
// empty list. Weather is the exception -- without it the calendar is still
// worth showing, just with the weather spot left blank. True once the new
// calendar is on the panel.
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
    err = time_sync::sync(kNtpSyncTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NTP sync failed (%s)", esp_err_to_name(err));
        reportOffline();
        return false;
    }
    gBoot.wifiFailures = 0;
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
    gBoot.offlineShown = false;
    installUpdateIfAny();
    return true;
}
}  // namespace

// Refresh once, then sleep until the next daily refresh -- or retry sooner
// if this one failed. A failed refresh may not even have a synced clock, so
// its retry is a plain delay rather than a time of day. Setup mode instead,
// if a long press asked for it or there's no WiFi to connect to.
extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "firmware %s, woke by %s", ota_update::runningVersion(),
             (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_TIMER)) ? "timer" : "reset/power-on");
    if (gBoot.magic != BootState::kMagic) {
        gBoot = {.magic = BootState::kMagic,
                 .enterSetup = false,
                 .accessPointSetup = false,
                 .wifiFailed = false,
                 .justConfigured = false,
                 .homeStepNext = false,
                 .pressed = false,
                 .offlineShown = false,
                 .wifiFailures = 0,
                 .lastPhoto = UINT8_MAX};
    }
    ESP_ERROR_CHECK(settings::initStorage());
    xTaskCreate(buttonTask, "button", 3072, nullptr, 5, nullptr);  // after NVS: a press writes to it
#if CONFIG_GLANCE_DEV_SERIAL_COMMANDS
    xTaskCreate(serialCommandTask, "serial_cmd", 3072, nullptr, 5, nullptr);
#endif
    Settings settings = settings::load();
    // Setup mode on a long press or with no WiFi saved -- and right after the
    // WiFi step while there are no calendars yet: a first setup goes on to
    // the home-network step. (Saving that with no calendars is fine: the
    // device is then a photo frame.)
    bool noCalendars = std::ranges::all_of(settings.icsUrls, [](const std::string& url) { return url.empty(); });
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
    // Got through a whole run without crashing: a fresh update has proven
    // itself even if the network was down.
    ota_update::markRunningAppValid();
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
    // and logs) and reboot at the same time deep sleep would have woken. The
    // button task handles presses meanwhile.
    // TODO(M9): wake from deep sleep on the button too.
    ESP_LOGW(kTag, "deep sleep disabled (menuconfig -> Glance Refresh), waiting awake instead");
    gPhase = Phase::waiting;
#if CONFIG_GLANCE_DEV_SETUP_ON_LAN
    if (setup_mode::startOnLan(settings) == ESP_OK) {
        ESP_LOGW(kTag, "development: setup pages served on the home network");
    }
#endif
    while (time(nullptr) < wakeAt) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (setup_mode::wasSaved()) {
            vTaskDelay(pdMS_TO_TICKS(1500));  // let the "saved" page reach the browser
            break;
        }
    }
    restartInto(false);
#endif
}
