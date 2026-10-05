#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "device_settings.hpp"

// The setup page served in setup mode: the HTML form, and turning a
// submitted form back into Settings. Pure C++, tested on the host; the
// access point and HTTP server around it are in components/setup_mode.
namespace setup_page {

// CWA's names for the counties and cities its forecast covers.
extern const std::span<const char* const> kCounties;

// Setup happens in two steps, so the phone has internet while it matters:
// on the access point only WiFi is set (renderWifiForm/applyWifiForm);
// calendars, weather and photos come after, from the home network
// (renderForm/applyForm), where the phone can still look up the URLs. The
// home-network form can change WiFi too, for moving to another network
// while the current one still works.
//
// Both forms come pre-filled with `current`. Secrets (WiFi password,
// calendar URLs, API key) are never sent back to the browser -- a set one
// shows as "已設定" and is kept unless replaced or explicitly removed.
// `errors` are shown on top; `nearbySsids` feeds the WiFi name suggestions.
// `revertedFrom` (if any) names a newly saved WiFi that didn't connect, so
// the device went back to the current one -- the page says so. Both end
// with a button that erases everything (POST /reset, confirm=yes).
std::string renderWifiForm(const Settings& current, std::span<const std::string> nearbySsids,
                           std::span<const std::string> errors, std::string_view revertedFrom = {});
std::string renderForm(const Settings& current, std::span<const std::string> nearbySsids,
                       std::span<const std::string> errors, std::string_view revertedFrom = {});

// Shown after a successful save, just before the device restarts.
std::string renderSaved();
// Shown after everything was erased, just before the device restarts.
std::string renderErased();
// Whether a POST /reset body confirms it (the button's hidden field).
bool confirmsReset(std::string_view body);

// The privacy-mode photos page (static; its script loads the list from
// /photos/list and does all the image processing).
std::string renderPhotosPage();
// GET /photos/list: {"max":20,"width":730,"height":280,"slots":[0,3]}.
std::string photosListJson(std::span<const int> slots, size_t max, int width, int height);

struct FormResult {
    Settings settings;
    std::vector<std::string> errors;  // empty = valid, safe to save
};

// Apply an application/x-www-form-urlencoded submission of the matching
// form on top of `current`; fields the form doesn't have stay as they are.
// A changed WiFi name drops the stored password: blank then means an open
// network.
FormResult applyWifiForm(const Settings& current, std::string_view body);
FormResult applyForm(const Settings& current, std::string_view body);

// The standard WiFi QR code payload ("WIFI:T:WPA;S:...;P:...;;") that phone
// cameras join directly.
std::string wifiQrPayload(std::string_view ssid, std::string_view password);

// A WPA2 password (8 characters, no look-alikes like 0/O or 1/l) for the
// setup access point, drawn from `random`.
std::string makeAccessPointPassword(const std::function<uint32_t()>& random);

// Exposed for tests.
std::string urlDecode(std::string_view encoded);
std::string htmlEscape(std::string_view text);

}  // namespace setup_page
