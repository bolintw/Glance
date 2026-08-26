# display_sim

PC-side viewer for the `SerialDumpDisplay` backend (M2). Lets you iterate on
rendering without the physical EPD attached -- the same UART used for
`idf.py monitor` also carries binary frame packets that this tool decodes
into an image.

Uses Tkinter (bundled with Python) instead of the PyQt/Dear PyGui suggested
in the dev plan, to keep the dependency list to just `pyserial` + `Pillow`.

## Setup

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Usage

1. Switch the firmware to the simulator backend:
   `idf.py menuconfig` -> Glance Display Backend -> Serial dump simulator.
2. Build and flash as usual (`idf.py build flash`).
3. Run this tool instead of `idf.py monitor` (don't run both at once -- only
   one process can hold the serial port):
   ```sh
   python3 display_sim.py /dev/cu.usbmodemXXXX
   ```
4. A window opens with the log on the right and the last rendered frame on
   the left; it updates each time firmware calls `flush()`.

## Protocol

Must stay in sync with
`firmware/components/serial_dump_display/serial_dump_display.hpp`:

```
[MAGIC "GLNC" 4B] [TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE]
```

- `PAYLOAD`: 1 bit per pixel, packed MSB-first, row-major. Bit 1 = white,
  0 = black (matches LVGL's `LV_COLOR_FORMAT_I1` default palette).
- `CHECKSUM`: 16-bit wraparound sum of the payload bytes.
- Anything outside a packet (i.e. not starting with `MAGIC`) is treated as
  plain log text and shown as-is.
