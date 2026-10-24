# Glance - Hardware

The board behind [Glance](../README.md): an ESP32-S3, a charger and fuel gauge for one LiPo cell, a buck-boost supply, and the drive circuit for a Waveshare 7.5" e-paper panel, on a 58 × 40 mm four-layer board.

## Design goals

The eCalendar is designed as an ultra-low-power, "glanceable" information appliance. The goal is a long battery life -- well over a year on a single charge -- by keeping the device asleep almost all the time and cutting power to everything it doesn't need then.

### Core principles
-   **Deep Sleep Dominance:** The device spends the vast majority of its time in a deep sleep state to conserve energy.
-   **One or two updates a day:** It wakes up at the set refresh times to connect to Wi-Fi, download the latest calendar data, and refresh the e-paper display.
-   **Switched Power Rails:** A key feature is the `V33_2` switched power rail, which completely cuts power to the e-paper panel's driver circuit and the status LED during sleep, eliminating their quiescent current draw.

### Minimal interaction
The device is a passive information radiator, not an interactive hub. Calendar management is handled on a user's phone or computer.
-   **Single, Multi-Function Button:** The single user button is for secondary tasks like toggling a "privacy mode" to hide event details or entering a one-time setup mode. It is not for daily navigation.
-   **No words on the panel's setup screens:** a QR code and an icon say what to do next; the phone's setup page explains.
-   **Exclusion of Additional Sensors:** Features like environmental sensors were intentionally excluded to maintain a focused product scope and avoid unnecessary power consumption.

## V3.0

V3.0 is the current board and the only one the firmware supports; its schematics are in [`v3.0/`](v3.0/). V2.1 (schematics in [`v2.1/`](v2.1/)) is retired: its USB/battery power-path MOSFET was fitted the wrong way round, so USB charged the battery directly, past the charger's 4.2V limit. Its last firmware is v0.2.0.

### Key components
| Component          | Value/Type                  | Description                                      |
|--------------------|-----------------------------|--------------------------------------------------|
| ESP32-S3 module    | ESP32-S3-WROOM-1-N8         | Main microcontroller with Wi-Fi and Bluetooth. 8MB flash, no PSRAM. GPIO35-37 are left free for a future module with Octal PSRAM. |
| MAX17048           | Fuel Gauge                  | Monitors the battery's voltage and state of charge via I2C; kept in hibernate (about 4µA). |
| TPS63802 (U3)      | Buck-boost converter        | Main 3.3V supply, from a 3.0V battery up to the ~4.6V USB path; about 11µA quiescent, automatic power-save mode. |
| TP4057             | Li-Ion Charger              | Charges the single-cell battery from USB at about 213mA (R15 = 4.7kΩ). |
| FPC connector      | 24-pin, 0.5mm pitch         | Connects to a Waveshare 7.5" 800×480 black-and-white e-paper panel, with its boost and charge-pump circuit on the board. |
| USB Type-C         | 16-pin                      | For power, charging and data (programming and the serial console, through the ESP32-S3's own USB). |

### Power
-   **Power Input:** USB Type-C or a 1S Li-Po battery (1000mAh, JST PH 2.0).
-   **Power path:** with USB in, the system runs from USB (through a Schottky diode) and the battery only charges through the TP4057; on battery, a P-MOSFET (Q2) passes it with little drop. Plugging or unplugging USB doesn't reset the device.
-   **Reverse-polarity protection:** a P-MOSFET (Q1) in series with the battery connector.
-   **USB and charge status:** VUSB through a 100k/100k divider on an ADC pin, and the TP4057's CHRG# output, both read at each wake-up.
-   **Switched rail:** `V33_2` (panel circuit, LED) switches through Q5/Q4 with a soft start; a pull-down keeps it off in deep sleep and during reset.
-   **USB-C Compatibility:** CC1/CC2 pins have 5.1kΩ pull-down resistors, ensuring compatibility with all USB-C cables and chargers.

### Buttons and LEDs
-   **Buttons:** `BOOT` (for flashing), `RST` (for reset), and a single multi-function `BTN` for user interaction (privacy/setup modes), plus a connector for an external button in parallel with it.
-   **LEDs:** A white general-purpose `LED` (on `V33_2`, active-low) and a red `CHG` indicator for battery charging status.

### ESP32-S3 pins

| Net Name  | ESP32-S3 GPIO | Description                               |
|-----------|---------------|-------------------------------------------|
| **Power** | | |
| PWR       | GPIO41        | Switches the `V33_2` rail (panel circuit, LED) |
| USB_DETEC | GPIO4         | VUSB through a divider (ADC1)             |
| CHG_STAT  | GPIO5         | The charger's CHRG# output                |
| **User Interface** | | |
| LED       | GPIO38        | White general-purpose LED                 |
| BTN       | GPIO1         | Button input (RTC GPIO, wakes from deep sleep) |
| BOOT      | GPIO0         | Boot button only                          |
| **E-Paper Display (SPI)** | | |
| MOSI      | GPIO10        | SPI data                                  |
| CLK       | GPIO11        | SPI clock                                 |
| EPD_CS    | GPIO12        | Chip select                               |
| EPD_DC    | GPIO13        | Data/command                              |
| EPD_RST   | GPIO14        | Reset                                     |
| EPD_BSY   | GPIO21        | Busy                                      |
| **I2C Bus (fuel gauge)** | | |
| SDA       | GPIO3         | I2C data                                  |
| SCL       | GPIO9         | I2C clock                                 |
| ALRT      | GPIO8         | Fuel gauge alert                          |
| **Clock and USB** | | |
| 32.768kHz | GPIO15 / GPIO16 | Crystal for the RTC through deep sleep  |
| D- / D+   | GPIO19 / GPIO20 | USB                                     |

Measured on V3.0 (a PPK2, checked against a multimeter): about 27 µA in deep sleep with the `V33_2` rail off.

## Versions

*   **v1.0:** first design: battery charger and gauge, buck-boost supply, USB-C, ESP32-S3-WROOM, external flash, a connector to an EPD driver board
*   **v2.0:** revised main and power schematics (2025-08-17)
*   **v2.1:** hardware-verified (2025-09-18), retired in October 2026 (see above); its last firmware is v0.2.0
*   **v3.0:** current revision (October 2026): the panel's drive circuit on the board, no external flash, USB and charge status inputs, a 32.768 kHz crystal

## License

This project is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License. See the [LICENSE](LICENSE) file for details.
