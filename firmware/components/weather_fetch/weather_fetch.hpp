#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "weather.hpp"

namespace weather_fetch {

// The forecast for `location` (a county, as CWA writes it) from CWA's
// 36-hour forecast: the 12-hour period containing `at` (see
// run_policy::forecastFor for which one a refresh shows). nullopt
// if apiKey or location is empty or the fetch fails; either way the
// calendar still shows. Needs an NTP-synced clock for TLS. Never logs the
// URL, which carries the API key.
std::optional<weather::Forecast> fetchForecast(int64_t at, const std::string& apiKey, const std::string& location);

}  // namespace weather_fetch
