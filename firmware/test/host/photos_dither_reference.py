#!/usr/bin/env python3
"""Writes dithering test vectors from tools/photos/bake_photos.py for
test_photos_dither.js: for each case, the 8-bit gray input and the packed
1bpp and 2bpp results. Run through `make dither-check`."""
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent / "tools" / "photos"))
import bake_photos  # noqa: E402
from PIL import Image, ImageOps  # noqa: E402


def cases():
    # A gradient with noise: exercises every level and lots of threshold ties.
    rnd = random.Random(1)
    w, h = 211, 97
    data = bytes(min(255, max(0, (x * 255) // (w - 1) + rnd.randint(-40, 40))) for y in range(h) for x in range(w))
    yield "gradient", w, h, data
    # The real thing, if the built-in photo's source is cached.
    photo = bake_photos.PHOTOS[0]
    if (bake_photos.CACHE / Path(photo.url).name).exists():
        img = Image.open(bake_photos.fetch(photo)).convert("L")
        img = ImageOps.fit(img, (bake_photos.WIDTH, bake_photos.HEIGHT), Image.LANCZOS, centering=(0.5, photo.center_y))
        img = ImageOps.autocontrast(img, cutoff=1)
        yield "great_wave", img.width, img.height, img.tobytes()


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    names = []
    for name, w, h, data in cases():
        img = Image.frombytes("L", (w, h), data)
        (out / f"{name}.gray").write_bytes(w.to_bytes(2, "little") + h.to_bytes(2, "little") + data)
        (out / f"{name}.mono").write_bytes(bake_photos.pack(bake_photos.atkinson_levels(img, 2), w, h, 1))
        (out / f"{name}.gray2").write_bytes(bake_photos.pack(bake_photos.atkinson_levels(img, 4), w, h, 2))
        names.append(name)
    (out / "cases.txt").write_text("\n".join(names) + "\n")


if __name__ == "__main__":
    main()
