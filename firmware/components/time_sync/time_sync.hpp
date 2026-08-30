#pragma once

#include <cstdint>

#include "esp_err.h"

// M3: NTP time sync. A free function rather than a class -- ESP-IDF's SNTP
// client is itself a global singleton with no real handle to manage, so
// wrapping it in a class would just be a class around nothing.
namespace time_sync {

// Sets the local timezone to Taiwan (UTC+8, no DST) and blocks until NTP
// sync completes or timeoutMs elapses. Requires an already-connected network
// interface (see WifiManager).
esp_err_t sync(uint32_t timeoutMs);

}  // namespace time_sync
