#pragma once

#include <array>
#include <cstdint>
#include <string>

// Everything the user sets up: stored in NVS (written by the setup page),
// with the menuconfig values from Kconfig.projbuild as a fallback for
// anything never saved there -- so a development board keeps working off
// sdkconfig until it's set up for real.
struct Settings {
    static constexpr size_t kMaxCalendars = 5;

    std::string wifiSsid;
    std::string wifiPassword;
    std::array<std::string, kMaxCalendars> icsUrls;  // empty = unused slot
    std::string cwaApiKey;                           // empty = no weather
    std::string weatherLocation;                     // e.g. "新竹縣"

    bool upsideDown = false;  // the panel drawn rotated 180 degrees

    // White on black (dark) or black on white (light), for the calendar and
    // the photo screens; with photoAutoTheme (on unless turned off) the
    // photo screens pick by the photo instead: light behind a dark photo,
    // dark behind a light one.
    enum class Theme : uint8_t { dark, light };
    Theme theme = Theme::dark;
    bool photoAutoTheme = true;

    // The daily refreshes, in minutes after local midnight: one, and an
    // optional second at least kMinRefreshGapMinutes from it either way
    // round the clock -- more than a failed refresh's retries take.
    static constexpr int kNoRefresh = -1;
    static constexpr int kMinRefreshGapMinutes = 3 * 60;
    int refreshMinutes = 7 * 60;
    int secondRefreshMinutes = kNoRefresh;

    // A QR code for guests to join a WiFi, in privacy mode: none, the
    // network the device uses, or another one (e.g. the router's guest network).
    enum class GuestWifi : uint8_t { off, own, other };
    GuestWifi guestWifi = GuestWifi::off;
    std::string guestSsid;
    std::string guestPassword;
};
