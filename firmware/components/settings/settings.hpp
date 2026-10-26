#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "device_settings.hpp"
#include "photo_rotation.hpp"
#include "esp_err.h"

// Settings itself is in device_settings.hpp, free of ESP-IDF headers so
// host-side code (the setup page) can use it too.

namespace settings {

// Initializes NVS (erasing it if its format is unusable). Call once, before
// load/save or anything else that uses NVS, such as WiFi -- and before
// anything else that could stop the run: it never fails for good, falling
// back to plaintext and then to an empty store.
//
// NVS is encrypted (CONFIG_NVS_ENCRYPTION, keys from an HMAC key the first
// encrypted init burns into eFuse): a flash dump shows no WiFi password or
// calendar URL. A device still holding plaintext NVS -- the key not burnt
// yet -- moves its settings over at its first boot after a proven update
// (a fresh one on probation keeps plaintext, so a rollback still reads it).
// This doesn't stop someone who flashes firmware of their own, which can
// ask the chip for the same keys: there's no secure boot.
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

// Everything NVS holds -- settings, the remembered WiFi, privacy mode, photo
// rotation, and the WiFi driver's own copy of the network -- gone: the
// partition is erased in flash, not just marked erased. Call at the start
// of a run, before WiFi: it reinitializes NVS. (Photos are photo_store's.)
esp_err_t eraseAll();

// When each calendar slot started failing to download (Unix time; 0 = it
// isn't), for the calendar-sync icon and the setup page. Saved only when a
// slot changes, not on every refresh.
using CalendarFailures = std::array<uint32_t, Settings::kMaxCalendars>;
CalendarFailures loadCalendarFailures();
esp_err_t saveCalendarFailures(const CalendarFailures& failures);

// Settings::panelFlipped alone, for drawing on the panel.
bool loadPanelFlipped();

// Privacy mode, toggled by the button. Kept apart from Settings so saving
// the setup page never touches it. Off if never saved.
bool loadPrivacyMode();
esp_err_t savePrivacyMode(bool on);

// Which uploaded photo privacy mode shows next (see photo_rotation).
photo_rotation::State loadPhotoRotation();
esp_err_t savePhotoRotation(const photo_rotation::State& state);

}  // namespace settings
