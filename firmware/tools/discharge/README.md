# Discharge tool

A separate app that drains a battery at full load down to a chosen resting
voltage, then sleeps with everything off -- to start a charge-curve
measurement from a known level without watching it.

Load: both cores spinning at 240MHz, Wi-Fi scanning nonstop with power save
off, the peripheral rail on (roughly 120-150mA). Every 5 minutes the load
stops for a minute and the fuel gauge reads the battery at rest: under load
the voltage sags, which would stop it early.

LED:
- a short blink every 2s: discharging
- five quick blinks, then off: done, asleep (a press of the button wakes it,
  and it goes straight back to sleep if the battery is still at the target)
- three quick blinks every 2s: the fuel gauge doesn't answer (it gives up
  after a minute)

## Build and flash

```sh
source firmware/env.sh
cd firmware/tools/discharge
idf.py menuconfig   # Discharge tool: target voltage (default 3500mV), pins
idf.py -p <port> flash
```

The defaults are for V2.1 (LED IO48, rail IO14); for V3.0 set the pins to
38 and 41. A target of 3000mV is "empty" by a cell's rated-capacity
definition: fine now and then, but charge it soon after.

For V3.0 in its own build directory:

```sh
idf.py -B build-v3.0 -D SDKCONFIG=build-v3.0/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board-v3.0" -p <port> flash
```

## Load steps (comparing boards)

With `sdkconfig.steps` layered last (`DISCHARGE_STEPS`), it doesn't
discharge: it cycles forever through 10s each of idle (CPU idle, Wi-Fi
stopped), both cores spinning, and both cores spinning with Wi-Fi scanning,
the peripheral rail off throughout. Record it with the PPK2 at a fixed
voltage on each board and compare the three levels: a difference that stays
the same at every step is something drawing whenever the chip is awake; one
that grows with the load is the regulator's efficiency.

It uses the firmware's own partition table and never opens NVS, so flashing
the firmware back (`idf.py flash` in `firmware/`) finds the device's settings,
wake log and photos as they were.

## V2.1: flash without the battery

V2.1's power-path MOSFET is reversed (dev plan risk #10): with USB in, USB
charges the battery directly. Flash with the battery out, then plug the
battery in and unplug USB. Without a battery the gauge reads the USB-fed
VBAT (~4.5V), so a flash on USB alone also works as a quick check: the LED
blinks and the log shows a reading every 6 minutes.
