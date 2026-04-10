#pragma once

namespace board {

// SPI Bus Pins
constexpr int MOSI = 35;
constexpr int MISO = 47;
constexpr int CLK  = 36;

// Chip Select Pins
constexpr int EPD_CS = 37;
constexpr int FLS_CS = 21;

// EPD Control Pins
constexpr int EPD_DC   = 38;
constexpr int EPD_RST  = 39;
constexpr int EPD_BUSY = 40;

// Power Control (V33_2 rail for EPD, Flash, LED)
constexpr int PWR  = 14;

// Peripherals & UI
constexpr int LED  = 48;
constexpr int BTN  = 1;
constexpr int BOOT = 0;

// I2C Bus (Fuel Gauge MAX17048)
constexpr int SDA  = 3;
constexpr int SCL  = 46;
constexpr int ALRT = 9;

// USB Interface
constexpr int USB_DN = 19;
constexpr int USB_DP = 20;

} // namespace board
