#pragma once

#include <cstdint>

#include "display.hpp"
#include "sdkconfig.h"

// Where things are on the eCalendar board (refs/private: the V3.0 change
// list), in one place. V3.0 is the only board: V2.1 was dropped on
// 2026-10-22 (its power path could charge the battery straight from USB,
// past the charger's limit). A new board adds a choice under Glance Board
// in menuconfig and its own block here.
namespace board {

inline constexpr FrameSize kPanelSize{.width = 800, .height = 480};
// The panel takes a write clock cycle down to 100ns (10MHz, spec 3.3-3);
// 4MHz leaves margin for the FPC. A frame is two 48KB planes: ~0.87s at 1MHz.
inline constexpr uint32_t kEpdSpiClockHz = 4'000'000;

// IO1: the button, with an external pull-up (R31) on the always-on rail, so
// it reads high through deep sleep until pressed.
inline constexpr int kButtonPin = 1;

inline constexpr const char* kName = "V3.0";
// The update manifest this board reads (.github/workflows/release.yml).
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

// IO4: VUSB through a 100k/100k divider (R3/R4), read on ADC1: about half
// of 5V with USB plugged in, near 0 on battery.
inline constexpr int kUsbDetectPin = 4;
// IO5: the charger's CHRG# through R8 (47k). With USB in, the charge LED
// (from VUSB) pulls it high; the charger pulls it low while charging. No
// pull-up of our own: the internal one and R8 would split it to ~1.7V.
// Meaningless without USB (the charger is unpowered).
inline constexpr int kChargeStatusPin = 5;

}  // namespace board
