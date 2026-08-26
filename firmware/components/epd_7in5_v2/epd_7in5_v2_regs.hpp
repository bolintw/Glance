#pragma once

#include <cstdint>

// Command/register table for the Waveshare 7.5" e-Paper V2 panel (SSD1683-
// family controller). Names match the panel datasheet
// (refs/private/datasheets/waveshare_7in5_v2_specification.pdf) so they stay
// cross-referenceable against it.
namespace epd {

enum class Command : uint8_t {
    PSR = 0x00,    // Panel Setting
    PWR = 0x01,    // Power Setting
    POF = 0x02,    // Power OFF
    PFS = 0x03,    // Power OFF Sequence Setting
    PON = 0x04,    // Power ON
    PMES = 0x05,   // Power ON Measure
    BTST = 0x06,   // Booster Soft Start
    DSLP = 0x07,   // Deep Sleep
    DTM1 = 0x10,   // Display Start Transmission 1
    DSP = 0x11,    // Data Stop
    DRF = 0x12,    // Display Refresh
    DTM2 = 0x13,   // Display Start Transmission 2
    DUSPI = 0x15,  // Dual SPI Mode
    AUTO = 0x17,   // Auto Sequence
    KWOPT = 0x2B,  // KW LUT Option
    PLL = 0x30,    // PLL Control
    TSC = 0x40,    // Temperature Sensor Calibration
    TSE = 0x41,    // Temperature Sensor Selection
    TSW = 0x42,    // Temperature Sensor Write
    TSR = 0x43,    // Temperature Sensor Read
    PBC = 0x44,    // Panel Break Check
    CDI = 0x50,    // VCOM and Data Interval Setting
    LPD = 0x51,    // Lower Power Detection
    EVS = 0x52,    // End Voltage Setting
    TCON = 0x60,   // TCON Setting
    TRES = 0x61,   // Resolution Setting
    GSST = 0x65,   // Gate/Source Start Setting
    REV = 0x70,    // Revision
    FLG = 0x71,    // Get Status
    AMV = 0x80,    // Auto Measurement VCOM
    VV = 0x81,     // Read VCOM Value
    VDCS = 0x82,   // VCOM_DC Setting
    PTL = 0x90,    // Partial Window
    PTIN = 0x91,   // Partial In
    PTOUT = 0x92,  // Partial Out
    PGM = 0xA0,    // Program Mode
    APG = 0xA1,    // Active Programming
    ROTP = 0xA2,   // Read OTP
    CCSET = 0xE0,  // Cascade Setting
    PWS = 0xE3,    // Power Saving
    LVSEL = 0xE4,  // LVD Voltage Select
    TSSET = 0xE5,  // Force Temperature
    TSBDRY = 0xE7  // Temperature Boundary Phase-C2
};

namespace Config {

namespace Panel {
inline constexpr uint8_t kResLutOtp = 0x00;
inline constexpr uint8_t kResLutReg = 0x80;
inline constexpr uint8_t kModeKwr = 0x00;
inline constexpr uint8_t kModeKw = 0x20;
inline constexpr uint8_t kScanDown = 0x00;
inline constexpr uint8_t kScanUp = 0x10;
inline constexpr uint8_t kShiftLeft = 0x00;
inline constexpr uint8_t kShiftRight = 0x08;
inline constexpr uint8_t kBoosterOff = 0x00;
inline constexpr uint8_t kBoosterOn = 0x04;
inline constexpr uint8_t kSoftReset = 0x00;
inline constexpr uint8_t kNoReset = 0x02;
}  // namespace Panel

namespace Power {
inline constexpr uint8_t kVgh20V = 0x07;
inline constexpr uint8_t kVglNeg20V = 0x07;
inline constexpr uint8_t kVdh15V = 0x3F;
inline constexpr uint8_t kVdlNeg15V = 0x3F;
}  // namespace Power

namespace Border {
inline constexpr uint8_t kFloating = 0xF7;
inline constexpr uint8_t kWhite = 0x17;
inline constexpr uint8_t kBlack = 0x10;
}  // namespace Border

namespace Sleep {
inline constexpr uint8_t kKeepRam = 0xA5;
inline constexpr uint8_t kClearRam = 0xA7;
}  // namespace Sleep

namespace Booster {
inline constexpr uint8_t kPhaseA = 0x17;
inline constexpr uint8_t kPhaseB = 0x17;
inline constexpr uint8_t kPhaseC = 0x28;
inline constexpr uint8_t kPhaseD = 0x17;
}  // namespace Booster

}  // namespace Config

}  // namespace epd
