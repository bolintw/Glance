#pragma once

#include <cstdint>

// MAX17048 fuel gauge (U14): its I2C address, registers and how to read
// them. Pure C++ for the host tests; components/fuel_gauge talks to it.
// Registers are 16-bit, big-endian on the wire.
namespace max17048 {

inline constexpr uint8_t kAddress = 0x36;

inline constexpr uint8_t kVcell = 0x02;    // 78.125 uV per LSB
inline constexpr uint8_t kSoc = 0x04;      // % in the high byte, 1/256 % in the low
inline constexpr uint8_t kVersion = 0x08;  // 0x001_
inline constexpr uint8_t kHibrt = 0x0A;    // HibThr | ActThr: when to hibernate
inline constexpr uint8_t kConfig = 0x0C;   // RCOMP | SLEEP ALSC ALRT ATHD[4:0]
inline constexpr uint8_t kCrate = 0x16;    // signed, 0.208 %/h per LSB
inline constexpr uint8_t kStatus = 0x1A;   // alert causes in the high byte

inline constexpr uint16_t kConfigAlert = 1 << 5;    // set with the ALRT pin pulled low
inline constexpr uint16_t kStatusAlerts = 0x3F00;   // RI VH VL VR HD SC: write 0 to clear
// Always hibernate: an ADC sample every 45s (~4uA) instead of every 250ms
// (~23uA). The default (0x8030) only hibernates after 6 minutes of a low
// charge rate, which a board measured on V2.1 never got to.
inline constexpr uint16_t kHibrtAlways = 0xFFFF;

inline constexpr uint16_t millivolts(uint16_t vcell) {
    return static_cast<uint16_t>((static_cast<uint32_t>(vcell) * 625 + 4000) / 8000);  // 78.125 uV = 625/8 uV
}

inline constexpr bool isVersion(uint16_t version) { return (version & 0xFFF0) == 0x0010; }

}  // namespace max17048
