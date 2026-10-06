#!/usr/bin/env python3
"""Checks the ESP32-S3-WROOM-1 pin use in an EasyEDA netlist against the
rules this board has learnt the hard way, and prints the pin map the
firmware needs. Standard library only.

    python3 check_pins.py netlist.json [--module U1]

The netlist is EasyEDA Pro's JSON netlist (Export -> Netlist, or
eda.sch_ManufactureData.getNetlistFile() through the bridge). Exit status
1 if any rule is broken.
"""
import argparse
import json
import sys

# Never connect: (gpio, why).
FORBIDDEN = {
    35: "Octal PSRAM (R8 modules): keep free so a PSRAM module drops in",
    36: "Octal PSRAM (R8 modules): keep free so a PSRAM module drops in",
    37: "Octal PSRAM (R8 modules): keep free so a PSRAM module drops in",
    45: "strapping: VDD_SPI voltage -- high at reset sets the flash to 1.8V and the module won't boot",
    46: "strapping: boot mode / ROM log -- keep off",
}
# Only for one job: gpio -> (allowed nets, why).
DEDICATED = {
    0: ({"BOOT"}, "strapping: download mode; only the BOOT button"),
    19: ({"D-"}, "native USB D-"),
    20: ({"D+"}, "native USB D+"),
}
# Allowed, but worth knowing.
NOTES = {
    3: "strapping (JTAG source); fine as I2C SDA (verified on V2.1)",
    15: "32.768kHz crystal (XTAL_32K_P) on this board",
    16: "32.768kHz crystal (XTAL_32K_N) on this board",
    39: "JTAG MTCK; free because debugging uses USB",
    40: "JTAG MTDO; free because debugging uses USB",
    41: "JTAG MTDI; free because debugging uses USB",
    42: "JTAG MTMS; free because debugging uses USB",
    43: "UART0 TX: the ROM prints boot messages on it -- nothing that must stay quiet at boot",
    44: "UART0 RX",
    47: "1.8V on modules with 1.8V flash/PSRAM ('V' suffix)",
    48: "1.8V on modules with 1.8V flash/PSRAM ('V' suffix)",
}
RTC_GPIOS = set(range(0, 22))   # deep-sleep wake sources
ADC1_GPIOS = set(range(1, 11))  # ADC2 (GPIO11-20) can't be read while WiFi is on
# Signals that must wake the chip from deep sleep, and ones read as analog.
MUST_WAKE = {"BTN", "USB_DETEC"}
ANALOG = {"USB_DETEC"}


def gpio_of(pin_name):
    """'IO38' -> 38; None for GND, 3V3, EN, EP..."""
    if pin_name.upper().startswith("IO") and pin_name[2:].isdigit():
        return int(pin_name[2:])
    return None


def load(path, designator):
    data = json.load(open(path, encoding="utf-8"))
    if "result" in data:  # saved straight from the bridge
        data = json.loads(data["result"])
    for comp in data["components"].values():
        if comp["props"].get("Designator") == designator:
            return comp
    sys.exit(f"no {designator} in the netlist")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("netlist")
    parser.add_argument("--module", default="U1")
    args = parser.parse_args()

    module = load(args.netlist, args.module)
    print(f"{args.module}: {module['props'].get('Manufacturer Part')}")
    used = {}  # gpio -> net
    for pin in module["pinInfoMap"].values():
        gpio = gpio_of(pin["name"])
        if gpio is not None and pin["net"]:
            used[gpio] = pin["net"]

    problems = []
    for gpio, why in FORBIDDEN.items():
        if gpio in used:
            problems.append(f"IO{gpio} is connected ({used[gpio]}): {why}")
    for gpio, (nets, why) in DEDICATED.items():
        if gpio in used and used[gpio] not in nets:
            problems.append(f"IO{gpio} carries {used[gpio]}, expected {'/'.join(sorted(nets))}: {why}")
    by_net = {net: gpio for gpio, net in used.items()}
    for net in sorted(MUST_WAKE):
        if net in by_net and by_net[net] not in RTC_GPIOS:
            problems.append(f"{net} is on IO{by_net[net]}, not an RTC GPIO (IO0-21): it can't wake the chip")
    for net in sorted(ANALOG):
        if net in by_net and by_net[net] not in ADC1_GPIOS:
            problems.append(f"{net} is on IO{by_net[net]}, not ADC1 (IO1-10): unreadable while WiFi is on")

    print("\nPin map (for the firmware's board config):")
    for gpio in sorted(used):
        note = NOTES.get(gpio, "")
        print(f"  IO{gpio:<3} {used[gpio]:<14} {note}")
    free_rtc = sorted(g for g in RTC_GPIOS - set(used) - set(DEDICATED) - {3, 15, 16})
    print("\nFree RTC GPIOs (can wake from deep sleep): " + ", ".join(f"IO{g}" for g in free_rtc))

    if problems:
        print("\nProblems:")
        for p in problems:
            print("  ✗ " + p)
        sys.exit(1)
    print("\nNo rule broken.")


if __name__ == "__main__":
    main()
