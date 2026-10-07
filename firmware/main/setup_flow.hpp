#pragma once

#include "device_settings.hpp"

namespace glance {

// Setup mode: on the home network when there's a WiFi to join, otherwise
// the WiFi step on the access point first. Ends in a restart -- after a
// save, or when nobody used it -- so the next boot starts clean.
[[noreturn]] void runSetupMode(const Settings& current);

}  // namespace glance
