# Project Glance: 2nd Gen E-Ink Calendar

## Project Overview
Glance is a custom-built, ultra-low-power electronic ink calendar. It is the second generation of a Raspberry Pi-based predecessor, now optimized for extreme battery life using a custom PCB and an ESP32-S3 microcontroller.

## Hardware Specifications (v2.1)
- **Microcontroller**: ESP32-S3-WROOM
- **Display**: Waveshare 7.5-inch monocolor E-Paper (800x480)
- **Power Management**:
  - **Target Sleep Current**: ~70uA
  - **Battery**: 1S 800mAh Li-ion
  - **Controlled Power Rail (V33_2)**: Managed via `PWR` pin (IO14). This rail powers the SPI Flash, EPD, and LED to eliminate leakage during deep sleep.
- **Peripherals**:
  - **Fuel Gauge**: MAX17048 (I2C) for battery monitoring.
  - **External Flash**: W25Q128JVSIQ (16MB).
  - **RTC**: External 32.768kHz oscillator for accurate timekeeping during sleep.
- **Documentation**:
  - Schematics and Netlist: `harware/v2.1/` (Note: directory is named `harware`)

## Software Workflow (Daily Cycle)
1. **Wake up**: Triggered by RTC timer (every 24 hours).
2. **Network**: Connect to WiFi.
3. **Time Sync**: Synchronize system time via NTP.
4. **Data Fetch**: Download `.ics` calendar file from a configured URL.
5. **Processing**: Parse the top 10 most relevant upcoming events.
6. **Rendering**:
   - Display current date and the 10 events on the EPD.
   - (Future) Add weather information or other widgets.
7. **Scheduled Wake**: Set the next wake-up timer for 24 hours later.
8. **Deep Sleep**: Power down the `V33_2` rail and enter ESP32 deep sleep.

## Key Files & Directories
- `Glance/main/`: Main application logic.
- `Glance/components/`: Hardware abstraction layers (GPIO, SPI, EPD driver wrapper).
- `third_party/waveshare/`: Original vendor drivers for the 7.5" E-Paper.
- `harware/v2.1/netlist.md`: Detailed pin mapping and circuit descriptions.

## Guidelines for Gemini
- **Power First**: Always prioritize power-efficient code. Ensure the `V33_2` rail is managed correctly.
- **Surgical Changes**: Adhere to existing component structures in `Glance/components/`.
- **Reference Vendor Code**: Use `third_party/waveshare/` as the reference for EPD sequences but adapt them to the project's SPI component.
