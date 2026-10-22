#!/usr/bin/env python3
"""Draws the setup screens' icons into src/setup/ (bake_icons.py then bakes
them into their own font, kSetupIcons). The panel's setup screens carry no
words: a QR code and an icon say what to do, the phone's page explains.
Supersampled, then thresholded to one bit like the panel. White is ink.

    ../font_subset/.venv/bin/python draw_setup_icons.py
"""
import math
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
OUT = HERE / "src" / "setup"
SS = 6


def canvas(w, h):
    img = Image.new("L", (w * SS, h * SS), 0)
    return img, ImageDraw.Draw(img)


def finish(img, w, h):
    return img.resize((w, h), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)


def capsule(d, x0, y0, x1, y1, width, fill=255):
    """A thick line with round ends."""
    d.line([x0, y0, x1, y1], fill=fill, width=int(width))
    r = width / 2
    for x, y in ((x0, y0), (x1, y1)):
        d.ellipse([x - r, y - r, x + r, y + r], fill=fill)


def wifi_arcs(d, cx, base, radius, stroke, rings=3):
    """WiFi waves fanning up from a dot at (cx, base)."""
    for k in range(rings):
        r = radius * (1 - k * 0.3)
        d.arc([cx - r, base - r, cx + r, base + r], 225, 315, fill=255, width=int(stroke))
    dot = stroke * 0.85
    d.ellipse([cx - dot, base - 2 * dot, cx + dot, base], fill=255)


def phone(d, x, y, w, h, stroke):
    """A phone's outline with a home bar, top-left at (x, y)."""
    d.rounded_rectangle([x, y, x + w, y + h], radius=w * 0.18, outline=255, width=int(stroke))
    bar = w * 0.3
    d.line([x + (w - bar) / 2, y + h - stroke * 2.2, x + (w + bar) / 2, y + h - stroke * 2.2], fill=255,
           width=int(stroke * 0.8))


def wifi():
    w, h = 170, 130
    img, d = canvas(w, h)
    s = SS
    wifi_arcs(d, w * s / 2, h * s * 0.97, h * s * 0.92, 15 * s)
    return finish(img, w, h)


def gear():
    w = h = 150
    img, d = canvas(w, h)
    s = SS
    c = w * s / 2
    teeth, r_out, r_body, r_hole = 8, 72 * s, 52 * s, 22 * s
    tooth = 24 * s
    for k in range(teeth):
        a = 2 * math.pi * k / teeth
        d.line([c, c, c + r_out * math.cos(a), c + r_out * math.sin(a)], fill=255, width=int(tooth))
    d.ellipse([c - r_body, c - r_body, c + r_body, c + r_body], fill=255)
    d.ellipse([c - r_hole, c - r_hole, c + r_hole, c + r_hole], fill=0)
    # Square off the tooth tips: cut everything past the outer radius.
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).ellipse([c - r_out, c - r_out, c + r_out, c + r_out], fill=255)
    img = Image.composite(img, Image.new("L", img.size, 0), mask)
    return finish(img, w, h)


def phone_hand():
    """A phone with a finger tapping it: carry on on the phone."""
    w, h = 190, 200
    img, d = canvas(w, h)
    s = SS
    stroke = 9 * s
    phone(d, 20 * s, 10 * s, 100 * s, 180 * s, stroke)
    # The finger, coming in from the lower right to the screen's middle.
    tip_x, tip_y = 82 * s, 92 * s
    capsule(d, tip_x, tip_y, 168 * s, 178 * s, 30 * s)
    d.line([tip_x + 10 * s, tip_y + 2 * s, tip_x + 2 * s, tip_y + 10 * s], fill=0, width=int(4 * s))  # the nail
    for r in (26 * s, 40 * s):  # the tap
        d.arc([tip_x - r, tip_y - r, tip_x + r, tip_y + r], 160, 290, fill=255, width=int(5 * s))
    return finish(img, w, h)


def two_phones():
    """Two phones on the access point: someone else got in."""
    w, h = 250, 210
    img, d = canvas(w, h)
    s = SS
    stroke = 8 * s
    for x in (25 * s, 145 * s):
        phone(d, x, 80 * s, 80 * s, 125 * s, stroke)
    wifi_arcs(d, w * s / 2, 66 * s, 62 * s, 11 * s)
    return finish(img, w, h)


def restart_button():
    """A circular arrow round a button: press the button to start over."""
    w = h = 120
    img, d = canvas(w, h)
    s = SS
    c, r = w * s / 2, 40 * s
    stroke = 10 * s
    d.arc([c - r, c - r, c + r, c + r], -60, 240, fill=255, width=int(stroke))
    # The arrowhead at the arc's end (240 degrees, clockwise from +x).
    a = math.radians(240)
    ex, ey = c + r * math.cos(a), c + r * math.sin(a)
    tx, ty = -math.sin(a), math.cos(a)  # the way the arc runs (clockwise on screen)
    nx, ny = math.cos(a), math.sin(a)  # outwards
    size = 28 * s
    tip = (ex + size * tx, ey + size * ty)
    outer = (ex + size * 0.6 * nx, ey + size * 0.6 * ny)
    inner = (ex - size * 0.6 * nx, ey - size * 0.6 * ny)
    d.polygon([outer, tip, inner], fill=255)
    b = 17 * s  # the button
    d.ellipse([c - b, c - b, c + b, c + b], fill=255)
    return finish(img, w, h)


def ship():
    """The frame's back with a finger on the button at its lower right."""
    w, h = 380, 260
    img, d = canvas(w, h)
    s = SS
    stroke = 9 * s
    left, top, right, bottom = 10 * s, 10 * s, 300 * s, 200 * s
    d.rounded_rectangle([left, top, right, bottom], radius=16 * s, outline=255, width=int(stroke))
    bx, by = right - 40 * s, bottom - 38 * s  # the button
    d.ellipse([bx - 15 * s, by - 15 * s, bx + 15 * s, by + 15 * s], fill=255)
    for r in (30 * s, 44 * s):  # pressing
        d.arc([bx - r, by - r, bx + r, by + r], 170, 280, fill=255, width=int(5 * s))
    capsule(d, bx + 10 * s, by + 10 * s, 356 * s, 236 * s, 34 * s)
    d.line([bx + 22 * s, by + 12 * s, bx + 12 * s, by + 22 * s], fill=0, width=int(5 * s))  # the nail
    return finish(img, w, h)


def small_wifi():
    w = h = 32
    img, d = canvas(w, h)
    s = SS
    wifi_arcs(d, w * s / 2, h * s * 0.92, h * s * 0.88, 4 * s)
    return finish(img, w, h)


def small_key():
    w = h = 32
    img, d = canvas(w, h)
    s = SS
    r = 8 * s
    cx, cy = 10 * s, 16 * s
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=int(4 * s))
    d.line([cx + r, cy, 30 * s, cy], fill=255, width=int(4 * s))
    for x in (23 * s, 28 * s):
        d.line([x, cy, x, cy + 7 * s], fill=255, width=int(4 * s))
    return finish(img, w, h)


def small_globe():
    w = h = 32
    img, d = canvas(w, h)
    s = SS
    c, r = 16 * s, 13 * s
    stroke = int(3 * s)
    d.ellipse([c - r, c - r, c + r, c + r], outline=255, width=stroke)
    d.ellipse([c - r * 0.45, c - r, c + r * 0.45, c + r], outline=255, width=stroke)
    d.line([c - r, c, c + r, c], fill=255, width=stroke)
    return finish(img, w, h)


ICONS = {
    "wifi.png": wifi,
    "gear.png": gear,
    "phone_hand.png": phone_hand,
    "two_phones.png": two_phones,
    "restart_button.png": restart_button,
    "ship.png": ship,
    "small_wifi.png": small_wifi,
    "small_key.png": small_key,
    "small_globe.png": small_globe,
}

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for name, draw in ICONS.items():
        draw().save(OUT / name)
    print(f"{len(ICONS)} icons in {OUT}")
