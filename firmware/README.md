# Glance Firmware

ESP-IDF v6.0 project for the eCalendar V2.1 board (ESP32-S3-WROOM-1-N8R8). See
`refs/private/ecalendar-v2-dev-plan.md` (not tracked in git) for the full
milestone plan this skeleton corresponds to (M0).

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

## Structure

- `main/` — app entry point (`app_main`), kept as a thin bootstrap.
- `components/` — project-local components go here, one directory per module
  (e.g. future `display`, `wifi_manager`, `epd_driver`). Empty for now — M0 is
  toolchain/skeleton only, no feature code yet.
- `sdkconfig.defaults` — board-specific config baked in (PSRAM disabled per
  the known GPIO35-37 conflict on this module, 16MB flash size). Run
  `idf.py set-target esp32s3` once after a fresh clone to generate `sdkconfig`.
