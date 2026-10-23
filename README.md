# Project Glance - an e-ink display calendar

This document provides an overview of the eCalendar board (V3.0), covering the project's design philosophy, hardware implementation, firmware features and security notes.

---

## 1. Design Philosophy

The eCalendar is designed as an ultra-low-power, "glanceable" information appliance. The primary goal is to achieve a battery life of over three months on a single charge by minimizing active time and power consumption.

### Core Principles
-   **Deep Sleep Dominance:** The device spends the vast majority of its time in a deep sleep state to conserve energy.
-   **Daily Update Cycle:** It wakes up once per day to connect to Wi-Fi, download the latest calendar data, and refresh the e-paper display.
-   **Switched Power Rails:** A key feature is the `V33_2` switched power rail, which completely cuts power to the e-paper panel's driver circuit and the status LED during sleep, eliminating their quiescent current draw.

### Minimalist Interaction Model
The device is a passive information radiator, not an interactive hub. Calendar management is handled on a user's phone or computer.
-   **Single, Multi-Function Button:** The single user button is for secondary tasks like toggling a "privacy mode" to hide event details or entering a one-time setup mode. It is not for daily navigation.
-   **No words on the panel's setup screens:** a QR code and an icon say what to do next; the phone's setup page explains.
-   **Exclusion of Additional Sensors:** Features like environmental sensors were intentionally excluded to maintain a focused product scope and avoid unnecessary power consumption.

---

## 2. Hardware Details

V3.0 is the current board and the only one the firmware supports. V2.1 (schematics in `hardware/v2.1/`) is retired: its USB/battery power-path MOSFET was fitted the wrong way round, so USB charged the battery directly, past the charger's 4.2V limit. Its last firmware is v0.2.0.

### Key Components
| Component          | Value/Type                  | Description                                      |
|--------------------|-----------------------------|--------------------------------------------------|
| ESP32-S3 module    | ESP32-S3-WROOM-1-N8         | Main microcontroller with Wi-Fi and Bluetooth. 8MB flash, no PSRAM. GPIO35-37 are left free for a future module with Octal PSRAM. |
| MAX17048           | Fuel Gauge                  | Monitors the battery's voltage and state of charge via I2C; kept in hibernate (about 4µA). |
| TPS63802 (U3)      | Buck-boost converter        | Main 3.3V supply, from a 3.0V battery up to the ~4.6V USB path; about 11µA quiescent, automatic power-save mode. |
| TP4057             | Li-Ion Charger              | Charges the single-cell battery from USB at about 213mA (R15 = 4.7kΩ). |
| FPC connector      | 24-pin, 0.5mm pitch         | Connects to a Waveshare 7.5" 800×480 black-and-white e-paper panel, with its boost and charge-pump circuit on the board. |
| USB Type-C         | 16-pin                      | For power, charging and data (programming and the serial console, through the ESP32-S3's own USB). |

### Power Subsystem
-   **Power Input:** USB Type-C or a 1S Li-Po battery (1000mAh, JST PH 2.0).
-   **Power path:** with USB in, the system runs from USB (through a Schottky diode) and the battery only charges through the TP4057; on battery, a P-MOSFET (Q2) passes it with little drop. Plugging or unplugging USB doesn't reset the device.
-   **Reverse-polarity protection:** a P-MOSFET (Q1) in series with the battery connector.
-   **USB and charge status:** VUSB through a 100k/100k divider on an ADC pin, and the TP4057's CHRG# output, both read at each wake-up.
-   **Switched rail:** `V33_2` (panel circuit, LED) switches through Q5/Q4 with a soft start; a pull-down keeps it off in deep sleep and during reset.
-   **USB-C Compatibility:** CC1/CC2 pins have 5.1kΩ pull-down resistors, ensuring compatibility with all USB-C cables and chargers.

### User Interface
-   **Buttons:** `BOOT` (for flashing), `RST` (for reset), and a single multi-function `BTN` for user interaction (privacy/setup modes), plus a connector for an external button in parallel with it.
-   **LEDs:** A white general-purpose `LED` (on `V33_2`, active-low) and a red `CHG` indicator for battery charging status.

### ESP32-S3 Connections

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

---

## 3. Firmware Features

The firmware (`firmware/`, ESP-IDF v6.0) wakes once a day, draws the panel and goes back to deep sleep. Everything else is set up from a phone.

-   **Calendar:** up to five private iCal (ICS) URLs, merged into the next ten events, shown as date and title only, two columns of five. Recurring events, exceptions and time zones are expanded on the device.
-   **Weather:** the daytime forecast for a Taiwan county from the Central Weather Administration (an API key is entered at setup).
-   **Daily refresh at 07:00**, or at a time set on the setup page (every half hour), with an optional second one at least 3 hours apart (a refresh flashes the panel for a few seconds, and a second one costs about a fifth more battery). Before noon a refresh shows today's daytime weather, from noon on tomorrow's. A failed refresh is retried hourly, at most three times in a row (07:00, 08:00, 09:00); then it waits for the next refresh time.
-   **The button:** a short press toggles privacy mode; a long press (3 s, the LED lights when it's long enough) opens setup mode. Held for 15 s (the LED blinks fast) and let go, it erases everything, like the setup page's reset: the way out when the setup page can't be reached. It wakes the device from deep sleep, and the LED blinks at once to show the press got through.
-   **Privacy mode:** date, weather and a photo instead of the events, for when guests are around. Switched on by a press, it is drawn at once without going online: the clock keeps time through deep sleep, and the weather is the forecast fetched earlier, as long as it's for today or later. Photos are uploaded from the setup page, cropped and dithered in the browser (zoom out to fit a whole photo, gaps filled white or black), and shown in 4 shades of gray. A fresh upload is shown next, then the photos rotate in shuffled rounds. Two Hokusai prints are built in. A QR code for guests to join a WiFi can take the weather's place: the device's own network, or another one such as the router's guest network (the code holds the password, so anyone who sees the panel can join).
-   **Photo frame:** with no calendar configured at all, the device always shows privacy mode's layout.
-   **Setup in two steps**, each opened by a QR code on the panel: first a WiFi access point to enter the home WiFi (a WiFi icon beside the QR code), then a page on the home network (a gear icon; with a one-time token) for calendars, weather, photos and the WiFi itself, so the phone keeps its internet while looking up calendar URLs. A second device on at the same time closes setup. A newly saved WiFi that doesn't connect falls back to the last one that did. In setup mode a short press does nothing and a long press leaves it. The panel can also be turned upside down there.
-   **Erasing everything** (on either setup page, after a confirmation page) readies the device to change hands: settings, photos and the wake log go, the panel shows a picture of a finger on the button at the back, and the device sleeps until that button wakes it into a first setup.
-   **Status icons** appear in the top-right corner only when something is wrong: a crossed-out WiFi (can't get online) and an empty battery (low). After a few failed refreshes, or right after setup, the panel switches to an offline screen (privacy mode's layout with the icon) rather than leaving a stale calendar up.
-   **Battery:** read from the MAX17048 fuel gauge each wake-up. Under 5% (about two months before it runs out) the icon shows and updates wait; under 2% (or 3.45V at rest) a "please charge" screen goes up and the device only checks the gauge hourly until it's back above 4% and 3.55V. With USB in, the wake log also notes whether it was charging.
-   **Wake log:** every wake-up records the battery, what the run did and how long it took, in a ring of 2048 entries on flash. The setup page offers it as CSV; `firmware/tools/battery/analyze_wake_log.py` turns it into battery life and clock drift.
-   **Updates:** after a successful refresh the device checks this repository's latest GitHub release and installs a newer `vX.Y.Z` build over HTTPS. A new build is kept only once it has read the release manifest itself -- proof it can still take the next update; if it crashes, restarts or sleeps before that, the device goes back to the previous build and skips this one. After a crash or a failed run the check comes first, before the calendars and weather, so data that trips the firmware can't keep a fix out. Releases are built by `.github/workflows/release.yml`.

Measured on V3.0 (PPK2, checked against a multimeter): about 27µA in deep sleep, and about 0.16mAh over 12s awake per refresh, so about 0.8mAh a day.

---

## Security Notes

Glance is a personal project, not a hardened product. This is what it protects and where it is known to fall short.

### Setup
-   **Step 1, WiFi (access point):** a WPA2 access point whose random 8-character password is shown only on the panel. Anyone who can see the panel while setup mode is open can join. The first device to join takes the QR code and password off the screen, and a second device on the access point at the same time closes it. A device that drops off and comes back with a new address (as iPhones may, with private Wi-Fi addresses) is taken for the same phone, so someone who copied the password before the QR code came down could slip in while the owner's phone is away. If someone else joins before the owner does, they get the page; the owner's phone joining while they're still on closes the access point, so the owner notices and starts over.
-   **Step 2, calendars, weather and photos (home network):** served over plain HTTP. The pages are gated by a 128-bit one-time token from the panel's QR code and bound to the first device that uses it, by address and a session cookie. The cookie is let in from a new address (the phone's private address may change), and the device is bound there from then on; anyone else who presents the token closes the pages. Not protected against:
    -   Someone on the same WiFi capturing the traffic. It is not encrypted, so calendar URLs and the API key travel in the clear, and members of a WPA2-PSK network can decrypt each other's traffic. That includes the session cookie, which is enough to use the pages from another device.
    -   Someone on the network spoofing the bound device's IP address.
-   Both steps close after 10 minutes without use. Neither protects against physical access: pressing the button and setting the device up again is the intended way in.

### Stored Secrets
-   The WiFi password (and that of the last network that connected, kept to fall back on when a newly saved one doesn't work), the private calendar URLs (each one grants read access to its calendar) and the CWA API key are stored in NVS **without flash encryption**. Anyone with the device and a USB cable can read them out with `esptool`. Flash encryption and secure boot would close this, but neither is configured.
-   The setup pages never send stored secrets back to the browser; a saved value shows only as "已設定".
-   "清除所有資料" on either setup page erases all of these, along with the uploaded photos and the wake log. It is a logical erase: the flash is not overwritten, so it does not defeat someone reading the chip afterwards. (Development builds then fall back to the values in their local `sdkconfig`.)

### Updates
-   OTA installs any newer `vX.Y.Z` release of this GitHub repository over certificate-checked HTTPS. Images are **not signed**, so whoever can publish a release here can update every device. App signing is not set up.
-   Rollback protects against an update that crashes or can't reach the next one, not against a malicious one.

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
