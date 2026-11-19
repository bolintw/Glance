#pragma once

// The setup pages' shared pieces: the style sheet, the icons, and the
// scripts. Plain strings, served inline -- the access point has no
// internet, so nothing may come from elsewhere (system fonts only).
namespace setup_page::assets {

// The style sheet, light and dark (it follows the phone's setting).
extern const char* const kStyle;

// Icon symbols (<use href="#i-cal"/> and so on), and the Glance logo.
extern const char* const kIcons;
extern const char* const kLogo;

// Helpers every page script starts with: $, $$, esc, btn.
extern const char* const kPrelude;

// The network picker (#nf under the chosen .net row): fills #ssid, shows the
// password field (#pw) only for networks that need one. Defines picker(),
// which returns a function that re-places it.
extern const char* const kPicker;

// The WiFi step: picks a network, then enables #go once it can be joined.
extern const char* const kWifiScript;

// The home-network form: switches views by #fragment, tracks what changed
// (the save bar), replaces or removes stored secrets, adds calendars,
// shows fields only for the choice they belong to (data-when), and checks
// for and installs updates.
extern const char* const kFormScript;

}  // namespace setup_page::assets
