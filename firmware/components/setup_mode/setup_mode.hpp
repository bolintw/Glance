#pragma once

#include <string>
#include <string_view>

#include "device_settings.hpp"
#include "esp_err.h"

// Setup mode's network side, in two steps (see components/setup_page):
//  - start(): the WiFi step, on an access point with a random password and
//    DNS that points every name at the device (a captive portal). Expects a
//    fresh boot -- it brings up WiFi itself, so WifiManager must not have run.
//  - startHome(): calendars, weather and photos, served on the home network
//    the device is connected to, so the phone keeps its internet. Only the
//    first device to open the URL from the panel's QR code (its one-time
//    token) gets in.
// Saving a valid form writes Settings to NVS. Setup mode is left by
// restarting the device.
namespace setup_mode {

struct AccessPoint {
    std::string ssid;      // "Glance-XXXX", from the MAC
    std::string password;  // fresh every time
    std::string url;       // where the setup page is, for typing in by hand
    std::string qrPayload;  // joins the access point when scanned
};

// Scans for nearby networks (for the page's suggestions), then starts the
// access point and both servers. `wifiError` (if any) is shown on the form,
// e.g. that the network saved last time didn't connect.
esp_err_t start(const Settings& current, AccessPoint& out, std::string_view wifiError = {});

struct HomeNetwork {
    std::string qrPayload;  // the page's URL, token included
};

// Starts the home-network step. WiFi must already be connected.
esp_err_t startHome(const Settings& current, HomeNetwork& out);

enum class Event {
    saved,     // valid settings were saved
    idle,      // nobody loaded a page for idleTimeoutMs
    joined,    // the first device got in: hide the QR code
    intruder,  // a second device got in (joined the access point, or used
               // the home-network secret): close up
    erased,    // "清除所有資料" was confirmed: erase everything (from a fresh
               // run, see eraseEverything) and start over
};

// Blocks until something happens in setup mode (see Event). `idle` also once
// `sessionLimitMs` has passed since setup mode started, used or not: a phone
// left joined to the access point keeps probing it, which would otherwise
// keep setup mode -- and the device -- up for good.
Event waitForEvent(uint32_t idleTimeoutMs, uint32_t sessionLimitMs);

// Closes up: drops every station and stops the access point, or stops
// serving the home-network pages.
void close();

// Erases every setting, uploaded photo and the wake log, in flash (the
// setup page's "清除所有資料", or the button held to glance::kEraseHoldMs).
// At the start of a run only, before WiFi: NVS is erased and set up anew.
void eraseEverything();

// Development aid (GLANCE_DEV_SETUP_ON_LAN): the same pages served on the
// home network the device is already connected to, so a computer on that
// network can drive them -- no access point, no DNS. Adds POST /restart.
esp_err_t startOnLan(const Settings& current);
// Whether the setup page saved settings or asked to erase everything
// (meant for polling in LAN mode); eraseAsked() tells the two apart.
bool wasSaved();
bool eraseAsked();

}  // namespace setup_mode
