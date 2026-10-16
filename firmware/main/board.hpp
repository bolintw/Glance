#pragma once

#include <cstdint>

#include "display.hpp"
#include "sdkconfig.h"

// Where things are on the eCalendar board (refs/private: hardware
// connections for V2.1, the V3.0 change list for V3.0), in one place. The
// board is chosen in menuconfig (Glance Board).
namespace board {

inline constexpr FrameSize kPanelSize{.width = 800, .height = 480};
// The panel takes a write clock cycle down to 100ns (10MHz, spec 3.3-3);
// 4MHz leaves margin for the FPC. A frame is two 48KB planes: ~0.87s at 1MHz.
inline constexpr uint32_t kEpdSpiClockHz = 4'000'000;

// IO1: the button, with an external pull-up (R31) on the always-on rail, so
// it reads high through deep sleep until pressed. Same on both boards.
inline constexpr int kButtonPin = 1;

#if CONFIG_GLANCE_BOARD_V3_0

inline constexpr const char* kName = "V3.0";
// Each board reads its own manifest, so a device never installs a build for
// the other board (.github/workflows/release.yml).
inline constexpr const char* kOtaManifest = "manifest-v3.0.json";

// IO38: status LED on the peripheral rail, lit by driving it low.
inline constexpr int kStatusLedPin = 38;
// IO41: switches the V33_2 rail (panel, LED) through Q5/Q4; R33 pulls it
// low, so the rail is off in deep sleep. High turns it on.
inline constexpr int kPeripheralRailPin = 41;

inline constexpr int kEpdMosiPin = 10;
inline constexpr int kEpdClkPin = 11;
inline constexpr int kEpdCsPin = 12;
inline constexpr int kEpdDcPin = 13;
inline constexpr int kEpdResetPin = 14;
inline constexpr int kEpdBusyPin = 21;
inline constexpr int kFlashCsPin = -1;  // no external flash

// IO4: VUSB through a 100k/100k divider (R3/R4), read on ADC1: about half
// of 5V with USB plugged in, near 0 on battery.
inline constexpr int kUsbDetectPin = 4;
// IO5: the charger's CHRG# through R8 (47k). With USB in, the charge LED
// (from VUSB) pulls it high; the charger pulls it low while charging. No
// pull-up of our own: the internal one and R8 would split it to ~1.7V.
// Meaningless without USB (the charger is unpowered).
inline constexpr int kChargeStatusPin = 5;

#else  // V2.1

inline constexpr const char* kName = "V2.1";
inline constexpr const char* kOtaManifest = "manifest.json";  // what V2.1 devices already read

// IO48: status LED, powered from the peripheral rail and lit by driving it
// low (per the schematic).
inline constexpr int kStatusLedPin = 48;
// GPIO14: switches the V33_2 rail that powers the EPD, external flash, and
// LED. It's off by default to save power in deep sleep -- must be driven
// high before the EPD (or anything else on that rail) will respond to
// anything, including SPI commands and the busy pin.
inline constexpr int kPeripheralRailPin = 14;

inline constexpr int kEpdMosiPin = 35;
inline constexpr int kEpdClkPin = 36;
inline constexpr int kEpdCsPin = 37;
inline constexpr int kEpdDcPin = 38;
inline constexpr int kEpdResetPin = 39;
inline constexpr int kEpdBusyPin = 40;
inline constexpr int kFlashCsPin = 21;  // external W25Q128, on the same rail and SPI lines

// No USB or charge status inputs.
inline constexpr int kUsbDetectPin = -1;
inline constexpr int kChargeStatusPin = -1;

#endif

}  // namespace board
