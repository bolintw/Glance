#pragma once

#include <cstdint>
#include <optional>

#include "weather.hpp"

namespace weather_fetch {

// Today's forecast for the configured county from CWA's 36-hour forecast
// (see weather::parseCwa36Hour for which period). nullopt if weather isn't
// configured or the fetch fails; either way the calendar still shows. Needs
// an NTP-synced clock for TLS. Never logs the URL, which carries the API key.
std::optional<weather::Forecast> fetchForecast(int64_t now);

}  // namespace weather_fetch
