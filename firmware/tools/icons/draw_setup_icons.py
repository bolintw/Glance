#!/usr/bin/env python3
"""Draws the setup screens' simple icons into src/setup/ (bake_icons.py then
bakes them into their own font, kSetupIcons); the hand-drawn illustrations
come from cut_setup_art.py. The panel's setup screens carry no words: a QR
code and a picture say what to do, the phone's page explains.
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


def wifi_arcs(d, cx, base, radius, stroke, rings=3):
    """WiFi waves fanning up from a dot at (cx, base)."""
    for k in range(rings):
        r = radius * (1 - k * 0.3)
        d.arc([cx - r, base - r, cx + r, base + r], 225, 315, fill=255, width=int(stroke))
    dot = stroke * 0.85
    d.ellipse([cx - dot, base - 2 * dot, cx + dot, base], fill=255)


def wifi():
    w, h = 110, 84
    img, d = canvas(w, h)
    s = SS
    wifi_arcs(d, w * s / 2, h * s * 0.97, h * s * 0.92, 10 * s)
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
    "small_wifi.png": small_wifi,
    "small_key.png": small_key,
    "small_globe.png": small_globe,
}

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for name, draw in ICONS.items():
        draw().save(OUT / name)
    print(f"{len(ICONS)} icons in {OUT}")
