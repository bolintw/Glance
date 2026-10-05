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


if __name__ == "__main__":
    wifi_off().save(HERE / "src" / "wifi_off.png")
