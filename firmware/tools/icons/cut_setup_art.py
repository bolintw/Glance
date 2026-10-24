"""Cuts the setup screens' illustrations, drawn with Gemini, into src/setup/,
scaled and thresholded to one bit like the panel; bake_icons.py then bakes
them. The hands come out of src/setup/gemini_sheet.png (a mock-up of all
six screens); the second-device and shipping screens are whole images of
their own, drawn to fill the panel's width. Each output is its own box, pen
at its bottom-left, as the drawn icons.

    ../font_subset/.venv/bin/python cut_setup_art.py
"""
from pathlib import Path

from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
OUT = HERE / "src" / "setup"

# Where each part comes from: a source image and the top-left of the part of
# it the boxes below count from (the sheet's panels are 503x250, laid out
# 2x3; the whole images are 1024x572).
PANELS = {
    "ap": ("gemini_sheet.png", 6, 3),
    "joined": ("gemini_sheet.png", 6, 258),
    "intruder": ("gemini_intruder.png", 0, 0),
    "ship": ("gemini_ship.png", 0, 0),
}

# (output, panel, box in panel coordinates, scale, boxes whose components are
# dropped; "bold" there thickens the lines first: thin ones
# would break up once scaled down.
ART = [
    ("scan_hand.png", "ap", (346, 140, 503, 250), 1.35, ()),
    ("tap_hand.png", "joined", (100, 45, 410, 250), 1.5, ()),
    ("intruder.png", "intruder", (0, 0, 1024, 572), 800 / 1024, ("bold",)),
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


def cut(panel, box, scale, drop):
    source, ox, oy = PANELS[panel]
    sheet = Image.open(OUT / source).convert("L")
    crop = sheet.crop((ox + box[0], oy + box[1], ox + box[2], oy + box[3]))
    if "bold" in drop:
        crop = crop.filter(ImageFilter.MaxFilter(3))
        drop = tuple(d for d in drop if d != "bold")
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
    for name, panel, box, scale, drop in ART:
        img = cut(panel, box, scale, drop)
        img.save(OUT / name)
        print(f"{name}: {img.width}x{img.height}")
