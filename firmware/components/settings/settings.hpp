#pragma once

#include <optional>
#include <string>

#include "device_settings.hpp"
#include "photo_rotation.hpp"
#include "esp_err.h"

// Settings itself is in device_settings.hpp, free of ESP-IDF headers so
// host-side code (the setup page) can use it too.

namespace settings {

// Initializes NVS (erasing it if its format is unusable). Call once, before
// load/save or anything else that uses NVS, such as WiFi.
esp_err_t initStorage();

// A key that was saved -- even as an empty string, e.g. a removed calendar
// -- wins over the menuconfig fallback.
Settings load();
esp_err_t save(const Settings& settings);

// Forgets the saved WiFi network and password, so the menuconfig ones apply
// again.
esp_err_t forgetWifi();

// The last WiFi network that connected, to fall back on when a newly saved
// one doesn't. remember() writes only when it changed (spares the flash).
struct WifiNetwork {
    std::string ssid;
    std::string password;
};
std::optional<WifiNetwork> loadWorkingWifi();
esp_err_t rememberWorkingWifi(const WifiNetwork& network);

// Puts `working` back as the saved WiFi after `failedSsid` didn't connect,
// and keeps a note of it for the setup page (takeWifiRevertNote: the
// failed name, or empty; cleared once taken).
esp_err_t revertWifi(const std::string& failedSsid, const WifiNetwork& working);
std::string takeWifiRevertNote();

// Everything this namespace holds -- settings, the remembered WiFi, privacy
// mode, photo rotation -- back to never saved. (Photos are photo_store's.)
esp_err_t eraseAll();

// Privacy mode, toggled by the button. Kept apart from Settings so saving
// the setup page never touches it. Off if never saved.
bool loadPrivacyMode();
esp_err_t savePrivacyMode(bool on);

// Which uploaded photo privacy mode shows next (see photo_rotation).
photo_rotation::State loadPhotoRotation();
esp_err_t savePhotoRotation(const photo_rotation::State& state);

}  // namespace settings
