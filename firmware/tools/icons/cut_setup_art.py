#!/usr/bin/env python3
"""Cuts the setup screens' illustrations, drawn with Gemini, into src/setup/,
scaled and thresholded to one bit like the panel; bake_icons.py then bakes
them. Most come out of src/setup/gemini_sheet.png (a mock-up of all six
screens), the shipping screen is src/setup/gemini_ship.png on its own. Each
image is its own box, pen at its bottom-left, as the drawn icons.

    ../font_subset/.venv/bin/python cut_setup_art.py
"""
from pathlib import Path

from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
OUT = HERE / "src" / "setup"
SHEET = OUT / "gemini_sheet.png"
SHIP = OUT / "gemini_ship.png"

# The sheet's panels (top-left corners), 503x250 each, laid out 2x3; "ship"
# is a sheet of its own, 1024x572, drawn to fill the panel's width.
PANELS = {"ap": (6, 3), "joined": (6, 258), "intruder": (6, 512), "ship": (0, 0)}

# (output, panel, box in panel coordinates, scale, boxes whose components are dropped)
ART = [
    ("scan_hand.png", "ap", (346, 140, 503, 250), 1.35, ()),
    ("tap_hand.png", "joined", (100, 45, 410, 250), 1.5, ()),
    ("phone_left.png", "intruder", (0, 20, 190, 165), 1.35, ()),
    ("phone_right.png", "intruder", (315, 5, 503, 120), 1.35, ()),
    ("wifi_medium.png", "intruder", (220, 58, 284, 100), 1.35, ()),
    ("cross.png", "intruder", (209, 100, 292, 173), 1.35, ()),
    # The frame's corner with a finger on the button, without the sheet's circular arrow.
    ("press_button.png", "intruder", (325, 132, 503, 251), 1.15, ((420, 178, 480, 233),)),
    ("ship.png", "ship", (0, 0, 1024, 572), 800 / 1024, ()),
]


def components(img, threshold=128, reach=3):
    """Connected bright regions (pixels within `reach` count as touching), as lists of (x, y)."""
    w, h = img.size
    px = img.load()
    seen, comps = set(), []
    for y in range(h):
        for x in range(w):
            if px[x, y] > threshold and (x, y) not in seen:
                stack, pts = [(x, y)], []
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    pts.append((cx, cy))
                    for dx in range(-reach, reach + 1):
                        for dy in range(-reach, reach + 1):
                            nx, ny = cx + dx, cy + dy
                            if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in seen and px[nx, ny] > threshold:
                                seen.add((nx, ny))
                                stack.append((nx, ny))
                comps.append(pts)
    return comps


def cut(sheets, panel, box, scale, drop):
    sheet = sheets["ship" if panel == "ship" else "sheet"]
    ox, oy = PANELS[panel]
    crop = sheet.crop((ox + box[0], oy + box[1], ox + box[2], oy + box[3]))
    if drop:
        px = crop.load()
        for pts in components(crop):
            xs = [p[0] + box[0] for p in pts]
            ys = [p[1] + box[1] for p in pts]
            if any(min(xs) >= d[0] and max(xs) <= d[2] and min(ys) >= d[1] and max(ys) <= d[3] for d in drop):
                for x, y in pts:
                    px[x, y] = 0
    w, h = crop.size
    big = crop.resize((round(w * scale), round(h * scale)), Image.LANCZOS).filter(ImageFilter.GaussianBlur(0.6))
    return big.point(lambda v: 255 if v > 120 else 0)


if __name__ == "__main__":
    sheets = {"sheet": Image.open(SHEET).convert("L"), "ship": Image.open(SHIP).convert("L")}
    for name, panel, box, scale, drop in ART:
        img = cut(sheets, panel, box, scale, drop)
        img.save(OUT / name)
        print(f"{name}: {img.width}x{img.height}")
