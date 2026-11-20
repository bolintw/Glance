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

    // The panel scanned the other way round (EpdConfig::upsideDown): the
    // way Glance is put together, so the default -- the shipping screen,
    // drawn with nothing set, comes out the right way up. The setup page's
    // 上下顛倒 turns it off for a frame that holds it the other way.
    bool panelFlipped = true;

    // The device stands upright (portrait, turned a quarter clockwise; with
    // panelFlipped off, counterclockwise) rather than on its side. The
    // calendar and the photos are laid out for it; privacy mode shows only
    // the photos of that shape.
    bool portrait = false;

    // White on black (dark) or black on white (light), for the calendar.
    enum class Theme : uint8_t { dark, light };
    Theme theme = Theme::dark;

    // The calendar shows each event's start and end between its date and
    // its title. Off by default: a glance needs the day and what it is, the
    // phone has the rest.
    bool eventTimes = false;

    // Privacy mode shows the built-in prints along with the uploaded photos.
    // Without any uploaded photos of the screen's shape it shows them anyway.
    // Off by default: with the prints mixed in, an upload seemed to vanish
    // on the days they came round.
    bool builtInPhotos = false;

    // Installs new releases by itself after a morning refresh. Off, the
    // setup page offers each one to install by hand (the manifest is still
    // read: that ends a fresh update's probation, see ota_update).
    bool autoUpdate = true;

    // The daily refreshes, in minutes after local midnight: one, and an
    // optional second at least kMinRefreshGapMinutes from it either way
    // round the clock -- more than a failed refresh's retries take.
    static constexpr int kNoRefresh = -1;
    static constexpr int kMinRefreshGapMinutes = 3 * 60;
    int refreshMinutes = 7 * 60;
    int secondRefreshMinutes = kNoRefresh;

    // A QR code in privacy mode's top-right corner: none, for guests to join
    // the WiFi the device uses or another one (e.g. the router's guest
    // network), or a link or any text of the user's (cornerText).
    enum class CornerQr : uint8_t { off, own, other, text };
    CornerQr cornerQr = CornerQr::off;
    std::string guestSsid;
    std::string guestPassword;
    std::string cornerText;
};
