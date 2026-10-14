# Glance Firmware

ESP-IDF v6.0 project for the eCalendar boards: V2.1 (ESP32-S3-WROOM-1-N8R8) and
V3.0 (ESP32-S3-WROOM-1-N8). What it does is described in the top-level README
(Firmware Features).

## Boards

The board is picked in `idf.py menuconfig` (Glance Board); its pins are in
`main/board.hpp`. A plain `idf.py build` is V2.1. For V3.0 in its own build
directory, layered on the same defaults:

```sh
idf.py -B build-v3.0 -D SDKCONFIG=build-v3.0/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board-v3.0" build
```

A release builds both, and each board reads its own update manifest
(`manifest.json` for V2.1, `manifest-v3.0.json` for V3.0), so a device never
installs the other board's build. V3.0 also logs whether USB was in and the
battery charging (`main/power_status`, the wake log's `power` column).

## Toolchain

Installed locally via the Espressif IDF installer at `~/.espressif`. Activate
it in each new shell before running `idf.py`:

```sh
source firmware/env.sh
idf.py build
idf.py -p <PORT> flash monitor
```

Always use `firmware/env.sh` (not the `eim`-generated
`~/.espressif/tools/activate_idf_v6.0.sh`) — the two scripts resolve the
project's Python virtualenv differently, and mixing them makes CMake refuse
to build with a "different Python env than configured" error. `env.sh` wraps
the official `export.sh`, so it resolves the same virtualenv `idf.py` would
pick on its own.

Confirmed with this toolchain (GCC 15.2.0 for xtensa-esp32s3):
- C: `-std=gnu23`
- C++: `-std=gnu++26` (auto-selected by ESP-IDF as the highest the compiler supports)

## Display backend

`main/peripherals.cpp` builds against either the real EPD driver or the
`SerialDumpDisplay` simulator (see `firmware/tools/display_sim/README.md`),
picked at compile time via the `Glance Display Backend` Kconfig choice.
Switch quickly without going through `idf.py menuconfig`:

```sh
./switch_display.sh sim   # no physical panel needed
./switch_display.sh epd   # panel must be wired up
idf.py build flash -p <PORT>
```

## Structure

- `main/` — one run of the device, split by concern:
  - `main.cpp`: `app_main`, the run from wake-up to sleep
  - `board.hpp`: every pin, in one place
  - `app_state`: the RTC boot state shared across restarts and deep sleep, this run's wake log entry, restart and deep sleep
  - `battery`: the fuel gauge reading, the charge reminder
  - `power_status`: USB and charging (V3.0)
  - `buttons`: what a press means (also on a wake from deep sleep)
  - `refresh`: the calendar, privacy mode, the offline screen
  - `setup_flow`: setup mode's two steps
  - `peripherals`: the panel, the status LED and their power rail
  - `serial_commands`: development builds only (see below)
- `components/` — project-local components, one directory per module. The ones
  without ESP-IDF dependencies (ICS parsing, calendar layout, setup pages,
  photo rotation, wake log format, `run_policy`'s retry and battery rules, ...)
  are also built and tested on the host.
- `sdkconfig.defaults` — config baked in (PSRAM disabled per the known
  GPIO35-37 conflict on V2.1's module, 8MB flash, custom partition table);
  `sdkconfig.board-v3.0` adds V3.0 on top. Run `idf.py set-target esp32s3`
  once after a fresh clone to generate `sdkconfig`.
- `partitions.csv` — two 3MB OTA app slots plus on-chip `storage` (uploaded
  photos and, in its last 64KB, the wake log). Changing it means a USB reflash
  (`idf.py erase-flash flash`), not an OTA update.

## Tests

```sh
make -C test/host                              # unit tests (host compiler, no board)
make -C test/host preview SCREEN=calendar      # build/preview.png; also setup, lan, joined,
                                               # intruder, privacy[1], gray[1], offline,
                                               # lowbattery, charge
make -C test/host dither-check                 # browser photo dithering vs tools/photos
```

## Development options

Off by default (release builds use the defaults); set in `idf.py menuconfig`:

- `GLANCE_DEEP_SLEEP` off: stay awake between refreshes so USB stays reachable.
- `GLANCE_DEV_SETUP_ON_LAN`: serve the setup pages on the home network with no
  token (see the README's security notes).
- `GLANCE_DEV_SERIAL_COMMANDS`: commands on the USB serial console —
  `glance press short|long`, `glance privacy on|off`, `glance refresh`,
  `glance refresh configured`, `glance wifi <name>|default`, `glance log`
  (the wake log as CSV), `glance sleep` (deep sleep until the button, for
  measuring sleep current), `glance battery <0-100>` (the next boot sees that
  state of charge), `glance hw` (a new board's check: slow clock, USB and
  charge status, fuel gauge, panel).

## Tools

- `tools/display_sim/capture.py` — capture a run's log (and frames, with the
  simulator backend); `--command` sends a serial command first.
- `tools/battery/analyze_wake_log.py` — battery life and clock drift from the
  wake log CSV.
- `tools/board/check_pins.py` — check an EasyEDA netlist against this board's
  ESP32-S3 pin rules and print the pin map.
- `tools/board/bringup.py` — a new board's first checks over USB: one run, then
  `glance hw` (crystal, USB and charge status, fuel gauge, panel), pass/fail.
- `tools/font_subset/`, `tools/icons/`, `tools/photos/` — regenerate the baked
  fonts, icons and built-in photos.