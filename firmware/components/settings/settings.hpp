#pragma once

#include "device_settings.hpp"
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

}  // namespace settings
