#!/usr/bin/env python3
"""A new board's first checks, from the USB serial console: restarts the
board, reads one run's log, asks for `glance hw` and says what passed. The
manual steps around it (V33 with a meter, the battery tests, sleep current)
are in refs/private, V3.0 change list, section 9.

    source firmware/env.sh
    tools/display_sim/.venv/bin/python tools/board/bringup.py /dev/cu.usbmodem3101 [--board V3.0]

Needs a development build (GLANCE_DEV_SERIAL_COMMANDS on, GLANCE_DEEP_SLEEP
off) for the board. Run it with USB plugged in, which is also what the USB
check expects. Exit status 1 if anything failed.
"""
import argparse
import os
import re
import sys
import time

import serial

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "display_sim"))
from capture import reset_board  # noqa: E402  (one reset that works on this board)

RUN_DONE = "waiting awake instead"  # main.cpp, development builds after a run
RUN_SECONDS = 90
HW_SECONDS = 20
# CHG_STAT's level has to be clear of the input thresholds (0.25/0.75 of
# 3.3V) to read reliably.
CHARGE_LOW_MAX_MV = 700
CHARGE_HIGH_MIN_MV = 2600


class Port:
    """The console, reopened as needed (a restart makes it vanish and return)."""

    def __init__(self, name):
        self.name, self.port, self.text = name, None, ""

    def read_until(self, marker, seconds):
        end = time.time() + seconds
        while time.time() < end:
            if self.port is None:
                try:
                    self.port = serial.Serial(self.name, 115200, timeout=0.2)
                except serial.SerialException:
                    time.sleep(0.3)
                    continue
            try:
                chunk = self.port.read(4096)
            except serial.SerialException:
                self.port = None
                continue
            self.text += chunk.decode(errors="replace")
            if marker and marker in self.text:
                return True
        return False

    def send(self, line):
        self.text = ""
        self.port.write((line + "\n").encode())
        self.port.flush()


class Report:
    def __init__(self):
        self.failed = False

    def check(self, ok, what, detail=""):
        print(f"  {'PASS' if ok else 'FAIL'}  {what}{': ' + detail if detail else ''}")
        self.failed |= not ok

    def note(self, what):
        print(f"  note  {what}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port")
    parser.add_argument("--board", help="the board it should report (V3.0); default: whatever it reports")
    parser.add_argument("--no-reset", action="store_true", help="don't restart it, just ask for the check")
    args = parser.parse_args()

    report = Report()
    console = Port(args.port)
    if not args.no_reset:
        print("restarting, one run:")
        reset_board(args.port)
        time.sleep(0.5)
        finished = console.read_until(RUN_DONE, RUN_SECONDS)
        log = console.text
        report.check(finished, "a whole run", "" if finished else f"no '{RUN_DONE}' within {RUN_SECONDS}s")
        errors = [line.strip() for line in log.splitlines() if line.startswith("E (")]
        report.check(not errors, "no errors logged", "; ".join(errors[:3]))
        refresh = re.search(r"refresh took (\d+)ms", log)
        report.check(refresh is not None, "the panel refreshed", f"{refresh.group(1)} ms" if refresh else "")
        if "brownout" in log.lower():
            report.check(False, "no brownout")
    else:
        console.read_until(None, 1)

    print("glance hw:")
    console.send("glance hw")
    if not console.read_until("hw: panel", HW_SECONDS):
        report.check(False, "glance hw answered", "no reply (a development build with serial commands?)")
        sys.exit(1)
    hw = "\n".join(line for line in console.text.splitlines() if line.startswith("hw:"))

    board = re.search(r"hw: board (\S+), firmware (\S+)", hw)
    if board:
        expected = args.board or board.group(1)
        report.check(board.group(1) == expected, "board", f"{board.group(1)} (firmware {board.group(2)})")
    else:
        report.check(False, "board")
    report.check("32.768kHz crystal" in hw, "32.768kHz crystal running",
                 re.search(r"hw: slow clock (.*)", hw).group(1) if "slow clock" in hw else "")

    power = re.search(r"hw: VUSB (-?\d+) mV, CHG_STAT (high|low) \((-?\d+) mV at the pin\): (.*)", hw)
    if power:
        vusb, level, charge_mv, verdict = int(power.group(1)), power.group(2), int(power.group(3)), power.group(4)
        report.check(vusb > 4500, "USB detected (running from USB)", f"VUSB {vusb} mV")
        clear = charge_mv <= CHARGE_LOW_MAX_MV if level == "low" else charge_mv >= CHARGE_HIGH_MIN_MV
        report.check(clear, "CHG_STAT level clear of the thresholds", f"{level}, {charge_mv} mV: {verdict}")
        report.note("with a battery in: 'charging' while the red LED is on, 'not charging' once it's off")
    else:
        report.check(False, "power status line")

    gauge = re.search(r"hw: fuel gauge (\d+) mV, ([\d.]+)%", hw)
    if gauge:
        mv = int(gauge.group(1))
        report.check(True, "fuel gauge answers", f"{mv} mV, {gauge.group(2)}%")
        if "hibernating" not in hw:
            report.note("fuel gauge not hibernating yet (~19uA more in sleep); it should be from the next boot on")
        if mv > 4300:
            report.note(f"{mv} mV is above a full cell: is a battery in?")
    else:
        report.check(False, "fuel gauge answers")
    report.check("hw: panel answers" in hw, "panel answers")

    print("FAILED" if report.failed else "all passed")
    sys.exit(1 if report.failed else 0)


if __name__ == "__main__":
    main()
