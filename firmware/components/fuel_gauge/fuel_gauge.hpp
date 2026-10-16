#pragma once

#include <cstdint>
#include <optional>

// The battery, as the MAX17048 fuel gauge (U14) sees it, over I2C (SDA IO3,
// SCL IO9). The gauge runs off the battery itself, so with no battery
// there's nothing to read.
namespace fuel_gauge {

struct Reading {
    uint16_t millivolts;
    uint16_t soc256;   // state of charge, % * 256
    int16_t crate;     // charge rate, 0.208 %/h per LSB (positive: charging)
    bool alertWasSet;  // the ALRT pin was pulled low (and has been released)
};

// Takes the bus, reads the gauge, clears any alert -- an asserted ALRT pin
// would leak ~330uA through its pull-up all night -- puts the gauge in
// hibernate for good (about 19uA less, all the time) and lets go of the bus.
std::optional<Reading> read();

}  // namespace fuel_gauge
