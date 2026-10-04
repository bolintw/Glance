#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The weather shown in the header, and how to read it out of CWA's 36-hour
// county forecast (dataset F-C0032-001). Pure C++, tested on the host; the
// HTTPS side is in components/weather_fetch.
namespace weather {

// One per icon, same set as the Raspberry Pi version.
enum class Condition { sunny, partlyCloudy, cloudy, windy, rainy };

struct Forecast {
    Condition condition;
    std::string description;  // CWA's wording, e.g. "陰時多雲短暫陣雨"
    int rainChance;           // percent
    int minTemp;              // Celsius
    int maxTemp;
};

// Buckets CWA's free-text weather description into an icon. Rain wins over
// everything ("多雲時陰短暫雷陣雨" is a rainy day); sun with clouds is partly
// cloudy.
Condition conditionOf(std::string_view description);

// Reads the 12-hour period that contains `now` (or the first one, if `now`
// is before them all) from a F-C0032-001 response for a single location.
// nullopt if any of Wx/PoP/MinT/MaxT is missing or malformed.
std::optional<Forecast> parseCwa36Hour(std::string_view json, int64_t now);

}  // namespace weather
