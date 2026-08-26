# display_sim

PC-side viewer for the `SerialDumpDisplay` backend (M2). Lets you iterate on
rendering without the physical EPD attached -- the same UART used for
`idf.py monitor` also carries frame data that this tool decodes into an
image.

Uses Tkinter (bundled with Python) instead of the PyQt/Dear PyGui suggested
in the dev plan, to keep the dependency list to just `pyserial` + `Pillow`.

## Setup

**Use a Python built against Tk 8.6+, not macOS's bundled system Python.**
The system Python (`/usr/bin/python3`) links against Apple's ancient,
deprecated Tk 8.5, which has broken color rendering under macOS Dark Mode --
widgets can render with unreadable/invisible colors regardless of what the
code requests, with no error of any kind (this cost a long debugging
session before the actual cause -- Tk version, not the protocol or the
GUI code -- was found). Get a modern one via Homebrew:

```sh
brew install python-tk@3.13   # pulls in tcl-tk (currently ships Tk 9.0)
/opt/homebrew/bin/python3.13 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 -c "import tkinter; print(tkinter.TkVersion)"   # should print 8.6 or higher
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
4. `app_main()` runs once and returns, so **press the board's physical RST
   button** after the window opens to get a fresh run/frame. There's a
   "Try software reset" button, but on this board (ESP32-S3's native
   USB-Serial-JTAG peripheral, over macOS) it's unreliable -- sometimes a
   no-op, and a different DTR+RTS sequence was found to reliably strand the
   chip in the ROM's "waiting for download" state rather than restart the
   app (recoverable with a physical RST). Don't build a workflow around it;
   the physical button always works.
5. A window opens with the log on the right and the last rendered frame on
   the left; it updates each time firmware calls `flush()`.

## Protocol

Must stay in sync with
`firmware/components/serial_dump_display/serial_dump_display.hpp`. A frame
is split across multiple plain-text lines rather than sent as one binary
blob or one giant line -- both of those were tried and turned out to be
unreliable on this hardware/OS combination (see the .cpp/.hpp comments for
the specifics: stdout's `\n` -> `\r\n` translation corrupting raw bytes,
a fixed-size TX ring buffer rejecting oversized single writes, and even a
single ~64KB plain-text line being unreliable). Each line looks like:

```
GLNC:<seq>:<total>:<base64 chunk>
```

Concatenating the base64 chunks for `seq` 0..`total-1` in order and
decoding the result gives:

```
[TYPE 1B] [LENGTH 4B LE] [PAYLOAD] [CHECKSUM 2B LE]
```

- `PAYLOAD`: 1 bit per pixel, packed MSB-first, row-major. Bit 1 = white,
  0 = black (matches LVGL's `LV_COLOR_FORMAT_I1` default palette).
- `CHECKSUM`: 16-bit wraparound sum of the payload bytes.
- Any line not starting with `GLNC:` is treated as plain log text and shown
  as-is.
