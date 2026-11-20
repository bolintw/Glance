#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "device_settings.hpp"
#include "wake_log.hpp"

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
// The device's state, shown at the top of the home-network form so a problem
// the panel only hints at (with a status icon, or not at all) can be read in
// full. Built from the wake log and the calendar failure times.
struct DeviceStatus {
    std::string firmwareVersion;  // e.g. "v0.2.4"; empty leaves it out
    bool releaseBuild = false;    // a vX.Y.Z release: updates can be checked for
    int64_t now = 0;  // 0 when the clock isn't set: dates are then left out
    int32_t utcOffset = 0;
    wake_log::Health health;  // over the last week
    std::array<uint32_t, Settings::kMaxCalendars> calendarFailingSince{};  // Unix time; 0 = fine
    int landscapePhotos = -1;  // uploaded photos of each shape; -1 = unknown
    int portraitPhotos = -1;
};

// A network the device found nearby, strongest first.
struct Network {
    std::string ssid;
    int rssi = -100;  // dBm
    bool open = false;
};

// `errors` are shown on top; `nearby` is the list of networks to pick from.
// `revertedFrom` (if any) names a newly saved WiFi that didn't connect, so
// the device went back to the current one -- the page says so. Both end
// with a link that leads to erasing everything, in two steps: GET /reset
// (renderResetConfirm), then its POST /reset, confirm=yes.
//
// The home-network form is one page made of views -- a list of sections,
// and each section on its own -- that its script switches between by the
// URL's #fragment, without asking the device for anything; all of them
// share one form, saved once. The photos view loads its script
// (photosScript, GET /photos.js) when first opened. Without script, every
// view shows, one after another.
// A page as it's written. Without a sink it's kept whole (str()); with one,
// it's handed over a couple of KB at a time, so the device never holds a
// whole page: the home-network form is ~50 KB, and a std::string growing to
// that needs one free block of 64 KB, which the heap often doesn't have
// (that aborted the device). Long constant parts (styles, scripts) go
// straight to the sink, uncopied. flush() hands over the rest.
class Html {
public:
    using Sink = std::function<void(std::string_view)>;
    Html() = default;
    explicit Html(Sink sink);
    Html& operator+=(std::string_view text);
    Html& operator+=(char c) { return *this += std::string_view(&c, 1); }
    void flush();
    std::string& str() { return buffer_; }

private:
    static constexpr size_t kChunk = 2048;
    Sink sink_;
    std::string buffer_;
};

std::string renderWifiForm(const Settings& current, std::span<const Network> nearby,
                           std::span<const std::string> errors, std::string_view revertedFrom = {});
std::string renderForm(const Settings& current, std::span<const Network> nearby,
                       std::span<const std::string> errors, std::string_view revertedFrom = {},
                       const DeviceStatus* status = nullptr);

// Shown after a successful save, just before the device restarts. On the
// WiFi step (`wifiStep`), what to do next: rejoin `ssid` and scan the next
// QR code.
std::string renderSaved(bool wifiStep, std::string_view ssid);
// The second step before erasing everything: says what goes, and asks.
std::string renderResetConfirm();
// Shown after everything was erased, just before the device restarts.
std::string renderErased();
// After 離開設定 (POST /exit): nothing saved, the device goes back to the calendar.
std::string renderLeft();
// For anyone but the phone that opened the home-network page first.
std::string renderDenied();
// The same pages, written to `html` (see Html) instead of returned whole.
void renderWifiForm(Html& html, const Settings& current, std::span<const Network> nearby,
                    std::span<const std::string> errors, std::string_view revertedFrom = {});
void renderForm(Html& html, const Settings& current, std::span<const Network> nearby,
                std::span<const std::string> errors, std::string_view revertedFrom = {},
                const DeviceStatus* status = nullptr);
void renderSaved(Html& html, bool wifiStep, std::string_view ssid);
void renderResetConfirm(Html& html);
void renderErased(Html& html);
void renderLeft(Html& html);
void renderDenied(Html& html);

// GET /update: whether a newer release can be installed by hand
// ({"current":"v0.6.0","latest":"v0.6.1","installable":true}), or why it
// couldn't be checked ({"error":"..."}, shown as is).
std::string updateJson(std::string_view current, std::string_view latest, bool installable);
std::string errorJson(std::string_view message);
// Whether a POST /reset body confirms it (the button's hidden field).
bool confirmsReset(std::string_view body);

// The photos view's script (GET /photos.js): loads the list from
// /photos/list and does all the image processing in the browser.
std::string_view photosScript();
// The photos view's markup, part of renderForm.
std::string_view photosView();
// A stored photo: its slot and size (800x480 landscape, 480x800 portrait).
struct StoredPhoto {
    int slot;
    int width;
    int height;
};
// GET /photos/list: {"max":20,"width":800,"height":480,"builtIn":true,
// "photos":[{"slot":0,"width":800,"height":480},...]} -- width and height
// are the screen's as the device stands now, the shape an upload is cropped
// to; builtIn is Settings::builtInPhotos.
std::string photosListJson(std::span<const StoredPhoto> photos, size_t max, int width, int height, bool builtIn);

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

// The standard WiFi QR code payload ("WIFI:T:WPA;S:...;P:...;;", or
// T:nopass without a password) that phone cameras join directly.
std::string wifiQrPayload(std::string_view ssid, std::string_view password);

// The payload for privacy mode's corner QR code, from settings.cornerQr: a
// WiFi to join, or the user's own link or text. Empty when there's none to
// show.
std::string cornerQrPayload(const Settings& settings);

// A WPA2 password (8 characters, no look-alikes like 0/O or 1/l) for the
// setup access point, drawn from `random`.
std::string makeAccessPointPassword(const std::function<uint32_t()>& random);

// Exposed for tests.
std::string urlDecode(std::string_view encoded);
std::string htmlEscape(std::string_view text);

}  // namespace setup_page
