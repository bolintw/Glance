# Glance Firmware

ESP-IDF v6.0 project for the Glance V3.0 board (ESP32-S3-WROOM-1-N8). The
top-level [README](../README.md) shows what Glance does from the outside;
[Features in detail](#features-in-detail) below is how, and
[Security Notes](#security-notes) what it does and doesn't protect.

## Boards

V3.0 is the only board: its pins are in `main/board.hpp`, and a plain
`idf.py build` (from `sdkconfig.defaults`) is the product build. V2.1 was
dropped on 2026-10-22 -- its power path could charge the battery straight
from USB -- and its last firmware is v0.2.0. A new board becomes another
choice under Glance Board in `idf.py menuconfig`.

For a development build (serial commands, no deep sleep -- what
`tools/board/bringup.py` needs), layer `sdkconfig.dev` on top, in its own
build directory:

```sh
idf.py -B build-dev -D SDKCONFIG=build-dev/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.dev" build
```

A device reads `manifest-v3.0.json` from the latest release for updates.
It also logs whether USB was in and the battery charging
(`main/power_status`, the wake log's `power` column).

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
- `sdkconfig.defaults` — the product config (V3.0, no PSRAM, 8MB flash,
  custom partition table); `sdkconfig.dev` adds the development options on
  top. Run `idf.py set-target esp32s3` once after a fresh clone to generate
  `sdkconfig`.
- `partitions.csv` — two 3MB OTA app slots plus on-chip `storage` (uploaded
  photos and, in its last 64KB, the wake log). Changing it means a USB reflash
  (`idf.py erase-flash flash`), not an OTA update.

## Tests

```sh
make -C test/host                              # unit tests (host compiler, no board)
make -C test/host preview SCREEN=calendar      # build/preview.png; also setup, lan, joined,
                                               # intruder, privacy[1], gray[1], guest[low], qrtext,
                                               # light, lightgray[1], offline, lowbattery,
                                               # charge
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

## Features in detail

The device wakes at its refresh times (once or twice a day), draws the panel and goes back to deep sleep. Everything else is set up from a phone.

-   **Calendar:** up to five private iCal (ICS) URLs, merged into the next ten events, shown as date and title only, two columns of five. Recurring events, exceptions and time zones are expanded on the device.
-   **Weather:** the daytime forecast for a Taiwan county from the Central Weather Administration (an API key is entered at setup).
-   **Daily refresh at 07:00**, or at a time set on the setup page (every half hour), with an optional second one at least 3 hours apart (a refresh flashes the panel for a few seconds, and a second one costs about a fifth more battery). Before noon a refresh shows today's daytime weather, from noon on tomorrow's. A failed refresh is retried hourly, at most three times in a row (07:00, 08:00, 09:00); then it waits for the next refresh time.
-   **The button:** a short press toggles privacy mode; a long press (3 s, the LED lights when it's long enough) opens setup mode. Held for 15 s (the LED blinks fast) and let go, it erases everything, like the setup page's reset: the way out when the setup page can't be reached. It wakes the device from deep sleep, and the LED blinks at once to show the press got through.
-   **Privacy mode:** date, weather and a photo instead of the events, for when guests are around. Switched on by a press, it is drawn at once without going online: the clock keeps time through deep sleep, and the weather is the forecast fetched earlier, as long as it's for today or later. Photos are uploaded from the setup page, cropped and dithered in the browser (zoom out to fit a whole photo, gaps filled white or black), and shown in 4 shades of gray. A fresh upload is shown next, then the photos rotate in shuffled rounds. Two Hokusai prints are built in. The calendar and the photo screens are white on black or, set on the setup page, black on white; the photo screens pick by the photo unless that's switched off (a light background behind a dark one). A QR code can take the weather's place: for guests to join the device's own WiFi or another one such as the router's guest network (the code holds the password, so anyone who sees the panel can join), or any link or text of up to 100 bytes (beyond that its modules would shrink below 3px in the 132px square).
-   **Photo frame:** with no calendar configured at all, the device always shows privacy mode's layout.
-   **Setup in two steps**, each opened by a QR code on the panel: first a WiFi access point to enter the home WiFi (a WiFi icon beside the QR code), then a page on the home network (a gear icon; with a one-time token) for calendars, weather, photos and the WiFi itself, so the phone keeps its internet while looking up calendar URLs. A second device on at the same time closes setup. A newly saved WiFi that doesn't connect falls back to the last one that did. In setup mode a short press does nothing and a long press leaves it. The panel can also be turned upside down there.
-   **Erasing everything** (on either setup page, after a confirmation page) readies the device to change hands: settings, photos and the wake log go, the panel shows a picture of a finger on the button at the back, and the device sleeps until that button wakes it into a first setup.
-   **Status icons** appear in the top-right corner only when something is wrong: a crossed-out WiFi (can't get online) and an empty battery (low). After a few failed refreshes, or right after setup, the panel switches to an offline screen (privacy mode's layout with the icon) rather than leaving a stale calendar up.
-   **Battery:** read from the MAX17048 fuel gauge each wake-up. Under 5% (about two months before it runs out) the icon shows and updates wait; under 2% (or 3.45V at rest) a "please charge" screen goes up and the device only checks the gauge hourly until it's back above 4% and 3.55V. With USB in, the wake log also notes whether it was charging.
-   **Wake log:** every wake-up records the battery, what the run did and how long it took, in a ring of 2048 entries on flash. The setup page offers it as CSV; `firmware/tools/battery/analyze_wake_log.py` turns it into battery life and clock drift.
-   **Updates:** after a successful refresh the device checks this repository's latest GitHub release and installs a newer `vX.Y.Z` build over HTTPS. A new build is kept only once it has read the release manifest itself -- proof it can still take the next update; if it crashes, restarts or sleeps before that, the device goes back to the previous build and skips this one. After a crash or a failed run the check comes first, before the calendars and weather, so data that trips the firmware can't keep a fix out. Releases are built by `.github/workflows/release.yml`.

Measured on V3.0 (PPK2, checked against a multimeter): about 27µA in deep sleep, and about 0.16mAh over 12s awake per refresh, so about 0.8mAh a day.

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
