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

// Blocks until settings were saved (true) or nobody has loaded a page for
// idleTimeoutMs (false).
bool waitForSave(uint32_t idleTimeoutMs);

}  // namespace setup_mode
