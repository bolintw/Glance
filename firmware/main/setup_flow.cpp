#include "setup_flow.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_state.hpp"
#include "board.hpp"
#include "canvas.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "peripherals.hpp"
#include "settings.hpp"
#include "setup_mode.hpp"
#include "setup_view.hpp"
#include "wifi_manager.hpp"

namespace glance {
namespace {

// Draws one of setup_view's screens onto the panel.
void showScreen(const std::function<void(Canvas&)>& draw) {
    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    draw(canvas);
    show(framebuffer);
}

// Setup mode gives up (and goes back to the calendar) after this long
// without anyone loading its page.
constexpr uint32_t kSetupIdleTimeoutMs = 10 * 60 * 1000;
// ...and after this long in any case (see setup_mode::waitForEvent).
constexpr uint32_t kSetupSessionLimitMs = 30 * 60 * 1000;

// Either setup step's events, until it ends in a restart.
[[noreturn]] void runSetupEvents(bool onAccessPoint, const std::string& accessPointUrl) {
    gPhase = Phase::setup;
    while (true) {
        switch (setup_mode::waitForEvent(kSetupIdleTimeoutMs, kSetupSessionLimitMs)) {
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
                // password) off the screen so nobody else can use them. On
                // the access point, the page's address stays in case it
                // didn't pop up.
                const std::string url = onAccessPoint ? accessPointUrl : std::string();
                showScreen([&](Canvas& canvas) { setup_view::renderPhoneJoined(canvas, url); });
                break;
            }
            case setup_mode::Event::intruder: {
                // A second device has the secret: close up. Setting up again
                // makes a new password or token.
                setup_mode::close();
                showScreen([](Canvas& canvas) { setup_view::renderIntruder(canvas); });
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
    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
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
    std::vector<uint8_t> framebuffer(board::kPanelSize.framebufferSize());
    Canvas canvas(framebuffer, board::kPanelSize);
    setup_view::renderLan(canvas, home.qrPayload);
    show(framebuffer);
    runSetupEvents(false, {});
}

}  // namespace

void runSetupMode(const Settings& current) {
    logRun(wake_log::Result::setup);
    if (gBoot.accessPointSetup || current.wifiSsid.empty()) {
        gBoot.accessPointSetup = false;
        runAccessPointSetup(current);
    }
    runHomeSetup(current);
}

}  // namespace glance
