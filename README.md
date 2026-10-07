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

### Known Issues (V2.1)
-   **The USB/battery power-path MOSFET (Q2) is fitted the wrong way round.** Its source is on VBAT and its drain on VSYS, so with USB plugged in the body diode feeds VSYS into the battery, bypassing the TP4057's 4.2V regulation: the battery was measured at 4.5V, and only its own protection board stopped it. Don't leave a battery charging unattended on V2.1. Fix on an existing board: replace Q2 with a Schottky diode from VBAT (anode) to VSYS (cathode), or rework it so the drain is on VBAT. V3.0 fixes it.
-   **The SPI bus uses GPIO35-37**, which the module's Octal PSRAM also uses, so PSRAM stays disabled. V3.0 moves the panel off those pins.
-   **R15 = 1.5kΩ sets about 667mA**, over the TP4057's 500mA rating; fit 4.7kΩ (about 213mA).

---

## 3. Firmware Features

The firmware (`firmware/`, ESP-IDF v6.0) wakes once a day, draws the panel and goes back to deep sleep. Everything else is set up from a phone.

-   **Calendar:** up to five private iCal (ICS) URLs, merged into the next ten events, shown as date and title only, two columns of five. Recurring events, exceptions and time zones are expanded on the device.
-   **Weather:** today's forecast for a Taiwan county from the Central Weather Administration (an API key is entered at setup).
-   **Daily refresh at 07:00.** A failed refresh is retried hourly, at most three times in a row (07:00, 08:00, 09:00); then it waits for the next day.
-   **The button:** a short press toggles privacy mode; a long press (3 s, the LED lights when it's long enough) opens setup mode. It wakes the device from deep sleep.
-   **Privacy mode:** date, weather and a photo instead of the events, for when guests are around. Photos are uploaded from the setup page, cropped and dithered in the browser (zoom out to fit a whole photo, gaps filled white or black), and shown in 4 shades of gray. A fresh upload is shown next, then the photos rotate in shuffled rounds. Two Hokusai prints are built in.
-   **Photo frame:** with no calendar configured at all, the device always shows privacy mode's layout.
-   **Setup in two steps**, each opened by a QR code on the panel: first a WiFi access point to enter the home WiFi, then a page on the home network (with a one-time token) for calendars, weather, photos and the WiFi itself, so the phone keeps its internet while looking up calendar URLs. A second device joining closes setup. A newly saved WiFi that doesn't connect falls back to the last one that did. Both pages can erase everything, after a confirmation page.
-   **Status icons** appear in the top-right corner only when something is wrong: a crossed-out WiFi (can't get online) and an empty battery (low). After a few failed refreshes, or right after setup, the panel switches to an offline screen (privacy mode's layout with the icon) rather than leaving a stale calendar up.
-   **Battery:** read from the MAX17048 fuel gauge each wake-up. Under 15% the icon shows and updates wait; under 5% (or 3.45V at rest) a "please charge" screen goes up and the device only checks the gauge hourly until it's back above 10% and 3.55V.
-   **Wake log:** every wake-up records the battery, what the run did and how long it took, in a ring of 2048 entries on flash. The setup page offers it as CSV; `firmware/tools/battery/analyze_wake_log.py` turns it into battery life and clock drift.
-   **Updates:** after a successful refresh the device checks this repository's latest GitHub release and installs a newer `vX.Y.Z` build over HTTPS, rolling back if it doesn't boot. Releases are built by `.github/workflows/release.yml`.

Measured on V2.1: about 66µA in deep sleep with the peripheral rail cut, about 15s awake per refresh.

---

## Security Notes

Glance is a personal project, not a hardened product. This is what it protects and where it is known to fall short.

### Setup
-   **Step 1, WiFi (access point):** a WPA2 access point whose random 8-character password is shown only on the panel. Anyone who can see the panel while setup mode is open can join. The first device to join takes the QR code and password off the screen, and a second device closes the access point. If someone else joins before the owner does, they get the page; the owner's phone then counts as the second device and closes the access point, so the owner notices and starts over.
-   **Step 2, calendars, weather and photos (home network):** served over plain HTTP. The pages are gated by a 128-bit one-time token from the panel's QR code and bound to the first device that uses it. Anyone else who presents the token or that session closes the pages. Not protected against:
    -   Someone on the same WiFi capturing the traffic. It is not encrypted, so calendar URLs and the API key travel in the clear, and members of a WPA2-PSK network can decrypt each other's traffic.
    -   Someone on the network spoofing the bound device's IP address.
-   Both steps close after 10 minutes without use. Neither protects against physical access: pressing the button and setting the device up again is the intended way in.

### Stored Secrets
-   The WiFi password (and that of the last network that connected, kept to fall back on when a newly saved one doesn't work), the private calendar URLs (each one grants read access to its calendar) and the CWA API key are stored in NVS **without flash encryption**. Anyone with the device and a USB cable can read them out with `esptool`. Flash encryption and secure boot would close this, but neither is configured.
-   The setup pages never send stored secrets back to the browser; a saved value shows only as "已設定".
-   "清除所有資料" on either setup page erases all of these, along with the uploaded photos. It is a logical erase: the flash is not overwritten, so it does not defeat someone reading the chip afterwards. (Development builds then fall back to the values in their local `sdkconfig`.)

### Updates
-   OTA installs any newer `vX.Y.Z` release of this GitHub repository over certificate-checked HTTPS. Images are **not signed**, so whoever can publish a release here can update every device. App signing is not set up.
-   Rollback protects against an update that crashes, not against a malicious one.

### Logs
-   The USB serial console is always on. Logs avoid WiFi passwords, calendar URLs and the API key (calendars appear by slot number). They do include **upcoming event titles** and the WiFi network name.

### Development Options
These are for development boards only. They are off by default, and release builds (`.github/workflows/release.yml`) use the defaults.
-   `GLANCE_DEV_SETUP_ON_LAN` serves the setup and photo pages on the home network with **no token at all**.
-   `GLANCE_DEV_SERIAL_COMMANDS` accepts button and WiFi commands on the USB serial console, and logs step 2's URL **including its token**.

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
