#pragma once

#include <cstdint>

#include "esp_err.h"

// M3: NTP time sync. A free function rather than a class -- ESP-IDF's SNTP
// client is itself a global singleton with no real handle to manage, so
// wrapping it in a class would just be a class around nothing.
namespace time_sync {

// The device's display zone (Taiwan, no DST), for code that does its own
// calendar math instead of going through libc's TZ.
inline constexpr int32_t kUtcOffsetSeconds = 8 * 3600;

// Sets the local timezone to Taiwan (UTC+8, no DST) and blocks until NTP
// sync completes or timeoutMs elapses. Requires an already-connected network
// interface (see WifiManager).
esp_err_t sync(uint32_t timeoutMs);

// Whether the clock holds a real date: synced on this boot, or kept by the
// RTC through a restart or deep sleep since an earlier sync. False after a
// power loss until the next sync.
bool clockIsPlausible();

}  // namespace time_sync
