# Project Glance - an e-ink display calendar

This document provides a comprehensive overview of the eCalendar board (V2.1), covering the project's design philosophy, hardware implementation, firmware architecture, and future considerations.

---

## 1. Design Philosophy

The eCalendar is designed as an ultra-low-power, "glanceable" information appliance. The primary goal is to achieve a battery life of over three months on a single charge by minimizing active time and power consumption.

### Core Principles
-   **Deep Sleep Dominance:** The device spends the vast majority of its time in a deep sleep state to conserve energy.
-   **Daily Update Cycle:** It wakes up once per day to connect to Wi-Fi, download the latest calendar data, and refresh the e-paper display.
-   **Switched Power Rails:** A key feature is the `V33_2` switched power rail, which completely cuts power to peripherals like the external flash and e-paper display during sleep, eliminating quiescent current draw.

### Minimalist Interaction Model
The device is a passive information radiator, not an interactive hub. Calendar management is handled on a user's phone or computer.
-   **Single, Multi-Function Button:** The single user button is for secondary tasks like toggling a "privacy mode" to hide event details or entering a one-time setup mode. It is not for daily navigation.
-   **Exclusion of Additional Sensors:** Features like environmental sensors were intentionally excluded to maintain a focused product scope and avoid unnecessary power consumption.

---

## 2. Hardware Details

### Key Components
| Designator | Component          | Value/Type                  | Description                                      |
|------------|--------------------|-----------------------------|--------------------------------------------------|
| U1         | ESP32-S3-wroom     | ESP32-S3-WROOM-1-N8R8       | Main microcontroller with Wi-Fi and Bluetooth. 8MB flash, Octal PSRAM. PSRAM conflicts with GPIO35-37, so it's currently disabled via menuconfig. |
| U14        | MAX17048G+T10      | Fuel Gauge                  | Monitors battery charge level via I2C.           |
| U4         | TPS63031DSKR       | Buck-boost converter        | Main 3.3V power supply for the board.            |
| U12        | TP4057             | Li-Ion Charger              | Manages charging of a single-cell Li-Ion battery.|
| U8         | W25Q128JVSIQ       | Flash Memory                | 128Mbit SPI flash for data storage.              |
| FPC1       | FPC Connector      | 24-pin, 0.5mm pitch         | Connects to an external Waveshare 7.5" 800*480 monocolor e-paper display. |
| USB1       | USB Type-C         | 16-pin                      | For power and data (programming/debugging).      |

### Power Subsystem
-   **Power Input:** USB Type-C or a single-cell Li-Ion battery.
-   **Charging:** The TP4057 (U12) charges the battery from USB power.
-   **Battery Management:** The MAX17048G+T10 (U14) provides accurate battery level monitoring.
-   **Voltage Regulation:** The TPS63031DSKR (U4) buck-boost converter provides a stable 3.3V, forced into Power-Save Mode for high efficiency.
-   **USB-C Compatibility:** CC1/CC2 pins have 5.1kΩ pull-down resistors, ensuring compatibility with all USB-C cables and chargers.

### User Interface
-   **Buttons:** `BOOT` (for flashing), `RST` (for reset), and a single multi-function `BTN` for user interaction (privacy/setup modes).
-   **LEDs:** A white general-purpose `LED` and a red `CHG` indicator for battery charging status. (Note: The white `LED` is active-low, meaning setting its GPIO to 0 turns it on).

### ESP32-S3 Core Connections
This table maps critical signals to their corresponding GPIO pins on the ESP32-S3 module (U1).

| Net Name  | Module Pin (U1) | ESP32-S3 GPIO | Description                               |
|-----------|-----------------|---------------|-------------------------------------------|
| **Power Control** | | | |
| PWR       | 22              | GPIO14        | Controls the power switch for `V33_2` rail (peripherals) |
| **User Interface** | | | |
| LED       | 25              | GPIO48        | Drives the white general-purpose LED      |
| BTN       | 39              | GPIO1         | User-programmable button input            |
| **E-Paper Display (SPI)** | | | |
| EPD_BSY   | 33              | GPIO40        | E-Paper Busy signal                       |
| EPD_RST   | 32              | GPIO39        | E-Paper Reset signal                      |
| EPD_DC    | 31              | GPIO38        | E-Paper Data/Command signal               |
| EPD_CS    | 30              | GPIO37        | E-Paper Chip Select                       |
| **External Flash (SPI)** | | | |
| FLS_CS    | 23              | GPIO21        | External Flash Chip Select                |
| **Shared SPI Bus** | | | |
| CLK       | 29              | GPIO36        | SPI Clock for E-Paper and Flash           |
| MOSI      | 28              | GPIO35        | SPI MOSI for E-Paper and Flash            |
| MISO      | 24              | GPIO47        | SPI MISO for Flash (E-Paper does not use) |
| **I2C Bus** | | | |
| SDA       | 15              | GPIO3         | I2C Data for Fuel Gauge                   |
| SCL       | 17              | GPIO9         | I2C Clock for Fuel Gauge                  |
| ALRT      | 18              | GPIO10        | Fuel Gauge Alert signal                   |
| **USB** | | | |
| D-        | 13              | USB_D- (GPIO19) | USB Negative signal                     |
| D+        | 14              | USB_D+ (GPIO20) | USB Positive signal                     |

---

## License

Copyright (c) 2025 Bo Lin

[![License: CC BY-NC-SA 4.0](https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You are free to:

- Share — copy and redistribute the material in any medium or format
- Adapt — remix, transform, and build upon the material

Under the following terms:

- Attribution — You must give appropriate credit to the original author (Bo Lin).
- NonCommercial — You may not use the material for commercial purposes.
- ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

No additional permissions are required for non-commercial reuse that respects these terms.

A copy of the full license is available at:
https://creativecommons.org/licenses/by-nc-sa/4.0/
