#pragma once

#include <array>
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
};
