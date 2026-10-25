#!/usr/bin/env python3
"""Draws the status icons into src/ (bake_icons.py then bakes them like the
weather icons). Unlike those, which were cut from the Raspberry Pi
version's images, these are drawn here: supersampled, then thresholded to
one bit like the panel. White is ink.

    ../font_subset/.venv/bin/python draw_status_icons.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SIZE = 24  # px; the top-right corner has room above the weather icon
SS = 8


def wifi_off():
    s = SIZE * SS
    img = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(img)
    cx, base = s / 2, s * 0.9
    stroke = int(s * 0.12)
    for r in (s * 0.9, s * 0.62, s * 0.34):
        d.arc([cx - r, base - r, cx + r, base + r], 225, 315, fill=255, width=stroke)
    dot = s * 0.09
    d.ellipse([cx - dot, base - 2 * dot, cx + dot, base], fill=255)
    # The slash, with a gap cut around it so it reads apart from the arcs.
    ends = [s * 0.1, s * 0.05, s * 0.9, s * 0.95]
    d.line(ends, fill=0, width=stroke * 3)
    d.line(ends, fill=255, width=stroke)
    return img.resize((SIZE, SIZE), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)


def battery_low():
    s = SIZE * SS
    img = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(img)
    stroke = int(s * 0.09)
    top, bottom = s * 0.27, s * 0.73  # centred, like the WiFi icon's weight
    body_right = s * 0.86
    d.rounded_rectangle([s * 0.04, top, body_right, bottom], radius=s * 0.06, outline=255, width=stroke)
    d.rectangle([body_right + stroke * 0.3, s * 0.4, s * 0.98, s * 0.6], fill=255)  # the terminal
    pad = stroke * 1.9
    d.rectangle([s * 0.04 + pad, top + pad, s * 0.04 + pad + s * 0.14, bottom - pad], fill=255)  # one bar left
    return img.resize((SIZE, SIZE), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)


def calendar_error():
    s = SIZE * SS
    img = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(img)
    stroke = int(s * 0.09)
    left, top, right, bottom = s * 0.08, s * 0.16, s * 0.92, s * 0.92
    d.rounded_rectangle([left, top, right, bottom], radius=s * 0.08, outline=255, width=stroke)
    d.rectangle([left, top, right, top + s * 0.18], fill=255)  # the header band
    for x in (s * 0.32, s * 0.68):  # the binding rings
        d.line([x, s * 0.04, x, s * 0.24], fill=255, width=stroke)
    # A cross in the page: the calendar didn't come through.
    x0, y0, x1, y1 = s * 0.32, s * 0.46, s * 0.68, s * 0.80
    d.line([x0, y0, x1, y1], fill=255, width=stroke)
    d.line([x0, y1, x1, y0], fill=255, width=stroke)
    return img.resize((SIZE, SIZE), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)


def weather_error():
    s = SIZE * SS
    img = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(img)
    stroke = int(s * 0.09)
    # A cloud outline: three bumps on a flat base, the inside cleared.
    parts = [
        ("ellipse", [s * 0.12, s * 0.44, s * 0.42, s * 0.76]),
        ("ellipse", [s * 0.28, s * 0.22, s * 0.68, s * 0.62]),
        ("ellipse", [s * 0.56, s * 0.38, s * 0.88, s * 0.72]),
        ("rect", [s * 0.27, s * 0.52, s * 0.74, s * 0.76]),
    ]
    for grow, fill in ((stroke * 2, 255), (0, 0)):
        for kind, (x0, y0, x1, y1) in parts:
            box = [x0 - grow / 2, y0 - grow / 2, x1 + grow / 2, y1 + grow / 2]
            (d.ellipse if kind == "ellipse" else d.rectangle)(box, fill=fill)
    # The slash, with a gap cut around it like the WiFi icon's.
    ends = [s * 0.1, s * 0.05, s * 0.9, s * 0.95]
    d.line(ends, fill=0, width=int(stroke * 3.2))
    d.line(ends, fill=255, width=int(stroke * 1.3))
    return img.resize((SIZE, SIZE), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)


if __name__ == "__main__":
    wifi_off().save(HERE / "src" / "wifi_off.png")
    battery_low().save(HERE / "src" / "battery_low.png")
    calendar_error().save(HERE / "src" / "calendar_error.png")
    weather_error().save(HERE / "src" / "weather_error.png")
