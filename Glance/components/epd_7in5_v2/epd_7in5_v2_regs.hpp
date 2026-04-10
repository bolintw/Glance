#pragma once

#include <cstdint>

namespace epd {

/**
 * @brief Command Table for 7.5inch e-Paper V2
 */
enum class Command : uint8_t {
    PSR     = 0x00, // Panel Setting
    PWR     = 0x01, // Power Setting
    POF     = 0x02, // Power OFF
    PFS     = 0x03, // Power OFF Sequence Setting
    PON     = 0x04, // Power ON
    PMES    = 0x05, // Power ON Measure
    BTST    = 0x06, // Booster Soft Start
    DSLP    = 0x07, // Deep Sleep
    DTM1    = 0x10, // Display Start Transmission 1
    DSP     = 0x11, // Data Stop
    DRF     = 0x12, // Display Refresh
    DTM2    = 0x13, // Display Start Transmission 2
    DUSPI   = 0x15, // Dual SPI Mode
    AUTO    = 0x17, // Auto Sequence
    KWOPT   = 0x2B, // KW LUT Option
    PLL     = 0x30, // PLL Control
    TSC     = 0x40, // Temperature Sensor Calibration
    TSE     = 0x41, // Temperature Sensor Selection
    TSW     = 0x42, // Temperature Sensor Write
    TSR     = 0x43, // Temperature Sensor Read
    PBC     = 0x44, // Panel Break Check
    CDI     = 0x50, // VCOM and Data Interval Setting
    LPD     = 0x51, // Lower Power Detection
    EVS     = 0x52, // End Voltage Setting
    TCON    = 0x60, // TCON Setting
    TRES    = 0x61, // Resolution Setting
    GSST    = 0x65, // Gate/Source Start Setting
    REV     = 0x70, // Revision
    FLG     = 0x71, // Get Status
    AMV     = 0x80, // Auto Measurement VCOM
    VV      = 0x81, // Read VCOM Value
    VDCS    = 0x82, // VCOM_DC Setting
    PTL     = 0x90, // Partial Window
    PTIN    = 0x91, // Partial In
    PTOUT   = 0x92, // Partial Out
    PGM     = 0xA0, // Program Mode
    APG     = 0xA1, // Active Programming
    ROTP    = 0xA2, // Read OTP
    CCSET   = 0xE0, // Cascade Setting
    PWS     = 0xE3, // Power Saving
    LVSEL   = 0xE4, // LVD Voltage Select
    TSSET   = 0xE5, // Force Temperature
    TSBDRY  = 0xE7  // Temperature Boundary Phase-C2
};

namespace Config {
    // PSR (Panel Setting Register)
    namespace Panel {
        inline constexpr uint8_t RES_LUT_OTP      = 0x00;
        inline constexpr uint8_t RES_LUT_REG      = 0x80;
        inline constexpr uint8_t MODE_KWR         = 0x00;
        inline constexpr uint8_t MODE_KW          = 0x20;
        inline constexpr uint8_t SCAN_DOWN        = 0x00;
        inline constexpr uint8_t SCAN_UP          = 0x10;
        inline constexpr uint8_t SHIFT_LEFT       = 0x00;
        inline constexpr uint8_t SHIFT_RIGHT      = 0x08;
        inline constexpr uint8_t BOOSTER_OFF      = 0x00;
        inline constexpr uint8_t BOOSTER_ON       = 0x04;
        inline constexpr uint8_t SOFT_RESET       = 0x00;
        inline constexpr uint8_t NO_RESET         = 0x02;
    }

    // PWR (Power Setting)
    namespace Power {
        inline constexpr uint8_t VGH_20V          = 0x07;
        inline constexpr uint8_t VGL_NEG_20V      = 0x07;
        inline constexpr uint8_t VDH_15V          = 0x3F;
        inline constexpr uint8_t VDL_NEG_15V      = 0x3F;
    }

    // CDI (VCOM and Data Interval Setting)
    namespace Border {
        inline constexpr uint8_t FLOATING         = 0xF7;
        inline constexpr uint8_t WHITE            = 0x17;
        inline constexpr uint8_t BLACK            = 0x10;
    }

    // Sleep Mode
    namespace Sleep {
        inline constexpr uint8_t KEEP_RAM         = 0xA5;
        inline constexpr uint8_t CLEAR_RAM        = 0xA7;
    }

    // Booster Soft Start (Common sequences)
    namespace Booster {
        inline constexpr uint8_t PHASE_A          = 0x17;
        inline constexpr uint8_t PHASE_B          = 0x17;
        inline constexpr uint8_t PHASE_C          = 0x28;
        inline constexpr uint8_t PHASE_D          = 0x17;
        
        inline constexpr uint8_t FAST_A           = 0x27;
        inline constexpr uint8_t FAST_B           = 0x27;
        inline constexpr uint8_t FAST_C           = 0x18;
    }
}

} // namespace epd
