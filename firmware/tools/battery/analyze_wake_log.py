#!/usr/bin/env python3
"""Battery life from the device's wake log (setup page -> 下載每次醒來的紀錄,
or `glance log` on a development build). Standard library only.

    python3 analyze_wake_log.py glance-wake-log.csv [--capacity-mah 800] [--sleep-ua 66]

Works on the latest stretch on battery: the log is cut wherever the state
of charge jumps up (a charge). The trend is a least-squares line through
state of charge over time, so a week or two is enough to extrapolate.
With --capacity-mah, it also splits the daily use into sleep (--sleep-ua,
measured: 66uA on V2.1) and the wake-ups.
"""
import argparse
import csv
import statistics
import sys
from collections import Counter
from datetime import datetime

CHARGE_JUMP_PCT = 2.0  # a rise this big between wake-ups means it was charged


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            row["when"] = datetime.strptime(row["time"], "%Y-%m-%d %H:%M:%S") if row["time"] else None
            rows.append(row)
    return rows


def number(row, key):
    return float(row[key]) if row.get(key) else None


def on_usb(row):
    """V3.0 logs whether USB was in (the power column); older logs can't tell."""
    return row.get("power") in ("charging", "usb")


def latest_discharge(rows):
    """The entries since the last charge that have a time and a battery reading.
    A charge is a run on USB (V3.0) or, without that column, a jump in the
    state of charge."""
    usable = [r for r in rows if r["when"] and number(r, "soc_pct") is not None]
    start = 0
    for i in range(len(usable)):
        if on_usb(usable[i]):
            start = i + 1
        elif i and number(usable[i], "soc_pct") - number(usable[i - 1], "soc_pct") > CHARGE_JUMP_PCT:
            start = i
    return usable[start:]


def slope_per_day(points):
    """Least-squares slope of (days, value) points."""
    xs, ys = zip(*points)
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in points) / sxx if sxx else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv")
    parser.add_argument("--capacity-mah", type=float, help="the battery's real capacity")
    parser.add_argument("--sleep-ua", type=float, default=66.0, help="measured deep-sleep current")
    args = parser.parse_args()

    rows = load(args.csv)
    if not rows:
        sys.exit("no entries")
    print(f"{len(rows)} wake-ups, seq {rows[0]['seq']}..{rows[-1]['seq']}")
    print("  by cause: " + ", ".join(f"{k} {v}" for k, v in Counter(r["wake"] for r in rows).most_common()))
    print("  by result: " + ", ".join(f"{k} {v}" for k, v in Counter(r["result"] for r in rows).most_common()))
    awake = [number(r, "awake_s") for r in rows]
    wifi = [number(r, "wifi_s") for r in rows if r["wifi_s"]]
    rssi = [number(r, "rssi_dbm") for r in rows if r["rssi_dbm"]]
    print(f"  awake: mean {statistics.fmean(awake):.1f} s, max {max(awake):.1f} s")
    if wifi:
        print(f"  WiFi: connects in {statistics.fmean(wifi):.2f} s on average, signal {statistics.fmean(rssi):.0f} dBm")

    drifts = []
    timed = [r for r in rows if r["when"]]
    for prev, cur in zip(timed, timed[1:]):
        if cur["clock_drift_ms"] and cur["wake"] == "timer":
            elapsed_ms = (cur["when"] - prev["when"]).total_seconds() * 1000
            if elapsed_ms > 3600 * 1000:  # short gaps say nothing about ppm
                drifts.append(number(cur, "clock_drift_ms") / elapsed_ms * 1e6)
    if drifts:
        ppm = statistics.fmean(drifts)
        print(f"  RTC drift: {ppm:+.2f} ppm ({ppm * 86.4:+.0f} ms/day; positive: the clock ran slow), "
              f"from {len(drifts)} timer wake-ups")

    stretch = latest_discharge(rows)
    if len(stretch) < 2:
        print("\nnot enough battery readings since the last charge for a trend")
        return
    t0 = stretch[0]["when"]
    days = (stretch[-1]["when"] - t0).total_seconds() / 86400
    points = [((r["when"] - t0).total_seconds() / 86400, number(r, "soc_pct")) for r in stretch]
    per_day = slope_per_day(points)
    first, last = stretch[0], stretch[-1]
    print(f"\nsince the last charge: {len(stretch)} wake-ups over {days:.1f} days")
    print(f"  {number(first, 'soc_pct'):.1f}% {number(first, 'battery_v'):.3f} V -> "
          f"{number(last, 'soc_pct'):.1f}% {number(last, 'battery_v'):.3f} V")
    if days < 2:
        print("  (under two days: the trend is still mostly noise)")
    if not per_day or per_day >= 0:
        print("  state of charge isn't falling (yet)")
        return
    remaining = number(last, "soc_pct")
    print(f"  falling {-per_day:.2f}%/day -> empty in about {remaining / -per_day:.0f} days "
          f"({100 / -per_day:.0f} days from full)")
    if args.capacity_mah:
        mah_day = -per_day / 100 * args.capacity_mah
        sleep_mah = args.sleep_ua * 24 / 1000
        wakes_per_day = (len(stretch) - 1) / days if days else 0
        waking = mah_day - sleep_mah
        print(f"  {mah_day:.2f} mAh/day: sleep {sleep_mah:.2f}, wake-ups {waking:.2f}"
              + (f" ({waking / wakes_per_day:.2f} mAh each, {wakes_per_day:.1f}/day)" if wakes_per_day else ""))
        if waking < 0:
            print("  (less than sleep alone: the capacity or the sleep current is off, or the trend is still noise)")


if __name__ == "__main__":
    main()
