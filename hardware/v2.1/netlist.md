# eCalendar Schematic Netlist & Connections

## Schematic 1: Main Controller & Peripherals (SCH_eCalendar_1-Main)

### 1. Microcontroller (U1: ESP32-S3-WROOM)
* **Power & Ground**
  * Pin 1, 40, 41 -> `GND`
  * Pin 2 (3V3) -> `V33`
* **Communication & Debug Interfaces**
  * Pin 13 (IO19) -> USB `D-`
  * Pin 14 (IO20) -> USB `D+`
  * Pin 36 (RXD0), Pin 37 (TXD0) -> Reserved for UART
* **I2C Bus (to Fuel Gauge)**
  * Pin 15 (IO3) -> `SDA`
  * Pin 16 (IO46) -> `SCL`
* **SPI Bus (Shared)**
  * Pin 28 -> `MOSI`
  * Pin 29 -> `CLK`
  * Pin 24 (IO47) -> `MISO` (Flash only)
* **Control & Status Pins**
  * Pin 3 (EN) -> `EN` (System Reset)
  * Pin 27 (IO0) -> `BOOT` (Boot/Flash mode)
  * Pin 25 (IO48) -> `LED` (Status Indicator)
  * Pin 39 (IO1) -> `BTN` (User Button)
  * Pin 17 (IO9) -> `ALRT` (Fuel Gauge Interrupt Alert)
  * Pin 22 (IO14) -> `PWR` (Controlled power switch for V33_2)
* **E-Paper (EPD) Dedicated Control Pins**
  * Pin 23 (IO21) -> `FLS_CS` (Flash Chip Select, distinct from EPD_CS)
  * Pin 30 (IO37) -> `EPD_CS` (E-Paper Chip Select)
  * Pin 31 (IO38) -> `EPD_DC` (E-Paper Data/Command)
  * Pin 32 (IO39) -> `EPD_RST` (E-Paper Reset)
  * Pin 33 (IO40) -> `EPD_BSY` (E-Paper Busy Status)
* **External RTC Oscillator (U2: 32.768kHz)**
  * Pin 8 (IO15), Pin 9 (IO16) are connected to the oscillator, paired with 18pF load capacitors (C15, C16) to GND.

### 2. External Memory (U8: W25Q128JVSIQ - SPI Flash)
* Pin 1 (CS#) -> `FLS_CS`
* Pin 2 (DO) -> `MISO`
* Pin 5 (DI) -> `MOSI`
* Pin 6 (CLK) -> `CLK`
* Pin 8 (VCC) -> `V33_2` (Controlled Power)
* Pin 4 (GND) -> `GND`

### 3. E-Paper FPC Interface (FPC1: 24-Pin)
* **Logic & Communication**: `EPD_BSY` (Pin 9), `EPD_RST` (Pin 10), `EPD_DC` (Pin 11), `EPD_CS` (Pin 12), `CLK` (Pin 13), `MOSI` (Pin 14)
* **Power**: `V33_2` (Pin 15, 16), `GND` (Pin 8, 17, 18)
* **Driver Control**: `GDR` (Pin 2), `RESE` (Pin 3)
* **Charge Pump / Voltage Boost-Buck**: Connected to multiple 1uF flying capacitors (C25-C32) and high/low voltage rails `VGH` (Pin 21), `VGL` (Pin 23).

### 4. Buttons & Interfaces
* **USB (TYPE-C 16PIN)**: VBUS -> `VUSB` / CC1, CC2 pulled down to GND via 5.1kΩ resistors / DP1, DP2 -> `D+` / DN1, DN2 -> `D-`.
* **RST Button**: Triggers `EN` to GND (with 100nF filter).
* **BOOT Button**: Triggers `BOOT` to GND (with 10kΩ pull-up to V33 and 100nF filter).
* **BTN Button**: Triggers `BTN` to GND (with 10kΩ pull-up to V33 and 100nF filter).
* **Indicator (LED)**: Anode to `V33_2`, cathode to `LED` (IO48 active-low) via a 330Ω current-limiting resistor.

---

## Schematic 2: Power Management & Driver Circuit (SCH_eCalendar_2-Power)

### 1. Battery Charge Management (U12: TP4057)
* **Input (VCC)**: Connected to `VUSB` (from Type-C).
* **Output (BAT)**: Connected to `VBAT` (Li-ion battery terminal).
* **Charging Indicator (CHRG#)**: Connected to a red LED from `VUSB` via a current-limiting resistor.
* **Program Setting (PROG)**: Connected to GND via a 1.5kΩ resistor to set the charging current.

### 2. Power Path Management
* **VUSB Supply**: Supplies `VSYS` via a Schottky diode (U13).
* **VBAT Supply**: Supplies `VSYS` via a P-Channel MOSFET (Q2). The Gate of Q2 is controlled by `VUSB` (Q2 is OFF when USB is plugged in; Q2 turns ON to supply battery power when USB is removed).

### 3. Main System Buck/Boost Converter (U4: TPS63031DSKR)
* **Input**: `VSYS` (connected to VIN, VINA, EN pins).
* **Output**: Regulated 3.3V to `V33`.
* **External Components**: Paired with a 1.5uH power inductor (U9) and 10uF input/output filter capacitors.

### 4. Controlled Power Switch (Generating V33_2)
* Composed of an N-Channel MOSFET (Q5) and a P-Channel MOSFET (Q4).
* When ESP32 outputs High on `PWR`, Q5 turns ON, which pulls the Gate of Q4 low, turning Q4 ON.
* **Function**: Switches `V33` power to `V33_2` to supply the Flash, E-Paper, and LED. This allows complete power cutoff to these components during deep sleep for extreme power saving.

### 5. Fuel Gauge (U14: MAX17048)
* **Power & Sensing**: VDD and CELL pins are connected to `VBAT`.
* **Communication**: `SDA`, `SCL` connected to the I2C bus (with 4.7kΩ pull-ups to V33).
* **Alert**: `ALRT` pin connected to MCU (with 10kΩ pull-up to V33).

### 6. E-Paper Panel Driver Circuit
* Driven by the `GDR` signal to an N-Channel MOSFET (Q3), switching the current through inductor L1 (10uH).
* The source terminal has current-sensing resistors (R25: 10kΩ, R23: 470mΩ) connected to `RESE` for controller feedback.
* Uses Schottky diodes (D1, D2, D3) and a capacitor network in a Charge Pump configuration to generate the high positive voltage `VGH` and negative voltage `VGL` required for E-Paper page turning.
