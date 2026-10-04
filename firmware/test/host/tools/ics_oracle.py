"""Reference expander for cross-checking the firmware's C++ ICS code.

Uses icalendar + recurring_ical_events (a mature, independent RFC 5545
implementation) to list the next occurrences that haven't ended yet, in the
same text format test_ics_events.cpp produces, and writes them under
fixtures/expected/. Re-run whenever the fixture calendar changes:

    python3 -m venv /tmp/ics-oracle && /tmp/ics-oracle/bin/pip install icalendar recurring_ical_events
    /tmp/ics-oracle/bin/python tools/ics_oracle.py      # from firmware/test/host

Review the diff of fixtures/expected/ before committing: this is the
reference, not gospel.
"""
import datetime as dt
from pathlib import Path

import icalendar
import recurring_ical_events

HERE = Path(__file__).resolve().parent.parent
FIXTURE = HERE / "fixtures" / "google_test_calendar.ics"
EXPECTED_DIR = HERE / "fixtures" / "expected"

TPE = dt.timezone(dt.timedelta(hours=8))
MAX_RESULTS = 200
HORIZON_DAYS = 120

# Taipei wall-clock "now" values; keep in sync with test_ics_events.cpp.
NOWS = [
    "2026-10-04T15:30",  # before everything starts
    "2026-10-13T09:00",  # mid multi-day all-day event, right before an override
    "2026-10-20T23:50",  # during the cross-midnight event
    "2026-11-25T12:00",  # first instances long past; split series switched over
    "2027-03-01T08:00",  # COUNT and UNTIL series finished
]


def to_local(v):
    if isinstance(v, dt.datetime):
        return v.replace(tzinfo=TPE) if v.tzinfo is None else v.astimezone(TPE)
    return dt.datetime(v.year, v.month, v.day, tzinfo=TPE)


def expand(cal, now):
    rows = []
    for ev in recurring_ical_events.of(cal).between(now, now + dt.timedelta(days=HORIZON_DAYS)):
        start_v = ev["DTSTART"].dt
        all_day = not isinstance(start_v, dt.datetime)
        start = to_local(start_v)
        if "DTEND" in ev:
            end = to_local(ev["DTEND"].dt)
        else:
            end = start + (dt.timedelta(days=1) if all_day else dt.timedelta(0))
        if end <= now:
            continue
        rows.append((start, str(ev.get("SUMMARY", "")), all_day))
    rows.sort(key=lambda r: (r[0], r[1]))
    out = []
    for start, summary, all_day in rows[:MAX_RESULTS]:
        when = start.strftime("%Y-%m-%d") + (" all-day" if all_day else start.strftime(" %H:%M"))
        out.append(f"{when} | {summary}")
    return out


def main():
    cal = icalendar.Calendar.from_ical(FIXTURE.read_bytes())
    EXPECTED_DIR.mkdir(exist_ok=True)
    for now_s in NOWS:
        now = dt.datetime.fromisoformat(now_s).replace(tzinfo=TPE)
        lines = expand(cal, now)
        (EXPECTED_DIR / f"{now_s.replace(':', '')}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"{now_s}: {len(lines)} occurrences")


main()
