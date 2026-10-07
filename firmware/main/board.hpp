#pragma once

#include <cstdint>

#include "display.hpp"

// Where things are on the eCalendar V2.1 board (refs/private, hardware
// connections), in one place. V3.0 moves most of these.
namespace board {

inline constexpr FrameSize kPanelSize{.width = 800, .height = 480};

// IO1: the button, with an external pull-up (R31) on the always-on rail, so
// it reads high through deep sleep until pressed.
inline constexpr int kButtonPin = 1;
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
inline constexpr uint32_t kEpdSpiClockHz = 1'000'000;

}  // namespace board
