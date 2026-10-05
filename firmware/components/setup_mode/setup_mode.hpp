#pragma once

#include <string>

#include "device_settings.hpp"
#include "esp_err.h"

// Setup mode's network side: an access point with a random password, DNS
// that points every name at the device, and the setup page (see
// components/setup_page) over HTTP. Saving a valid form writes Settings to
// NVS. Expects a fresh boot -- it brings up WiFi itself, so WifiManager must
// not have run -- and is left by restarting the device.
namespace setup_mode {

struct AccessPoint {
    std::string ssid;      // "Glance-XXXX", from the MAC
    std::string password;  // fresh every time
    std::string url;       // where the setup page is, for typing in by hand
    std::string qrPayload;  // joins the access point when scanned
};

// Scans for nearby networks (for the page's suggestions), then starts the
// access point and both servers.
esp_err_t start(const Settings& current, AccessPoint& out);

enum class Event {
    saved,     // valid settings were saved
    idle,      // nobody loaded a page for idleTimeoutMs
    joined,    // the first device joined the access point: hide the QR code
    intruder,  // a second device joined: close the access point
};

// Blocks until something happens in setup mode (see Event).
Event waitForEvent(uint32_t idleTimeoutMs);

// Drops every station and stops the access point.
void closeAccessPoint();

// Development aid (GLANCE_DEV_SETUP_ON_LAN): the same pages served on the
// home network the device is already connected to, so a computer on that
// network can drive them -- no access point, no DNS. Adds POST /restart.
esp_err_t startOnLan(const Settings& current);
// Whether the setup page saved settings (meant for polling in LAN mode).
bool wasSaved();

}  // namespace setup_mode
