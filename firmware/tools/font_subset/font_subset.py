"""Bakes Noto Sans TC into the bitmap fonts the firmware draws with.

Each entry in FONTS becomes components/fonts/<name>.cpp: a flash-resident
Glyph table plus 1bpp bitmaps for exactly the characters that font needs.
Big sizes (the day number, the weekday) only carry the handful of glyphs
they show, so only the event-list font pays for the full character set.

    python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
    .venv/bin/python font_subset.py

The font is downloaded into .cache/ on first run (gitignored); its SHA-256
is pinned so an upstream change can't silently alter the output.
"""
import hashlib
import re
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
CACHE = HERE / ".cache"
OUT_DIR = HERE.parent.parent / "components" / "fonts"

FONT_URL = "https://github.com/google/fonts/raw/main/ofl/notosanstc/NotoSansTC%5Bwght%5D.ttf"
FONT_SHA256 = "864727d210d54f2537bbe23b3a839436c3992af72de9322af5270897246bd44f"
LICENSE_URL = "https://raw.githubusercontent.com/google/fonts/main/ofl/notosanstc/OFL.txt"

# Pixels with antialiased coverage at or above this become ink. Thresholding
# an antialiased render keeps stroke shapes more faithful than FreeType's
# hinted monochrome mode, which Noto CJK isn't hinted for.
INK_THRESHOLD = 128


def big5_range(first, last):
    """Characters encoded in Big5 between two code values, in Big5 order."""
    chars = []
    for lead in range(first >> 8, (last >> 8) + 1):
        for trail in list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)):
            code = (lead << 8) | trail
            if first <= code <= last:
                try:
                    chars.append(bytes([lead, trail]).decode("big5"))
                except UnicodeDecodeError:
                    pass
    return "".join(chars)


ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
# Big5's symbol block: full-width punctuation, brackets, bopomofo, ...
BIG5_SYMBOLS = big5_range(0xA140, 0xA3BF)
# Big5 level 1, the 5401 frequently used hanzi (laid out after the Ministry
# of Education's common-character standard).
BIG5_COMMON_HANZI = big5_range(0xA440, 0xC67E)
EXTRA_PUNCTUATION = "…—–‘’“”•·～"

CALENDAR_TEXT = ASCII + BIG5_SYMBOLS + BIG5_COMMON_HANZI + EXTRA_PUNCTUATION
DIGITS = "0123456789"
WEEKDAYS = "一二三四五六日"
MONTH_ABBREVIATIONS = "Jan.Feb.Mar.Apr.May.Jun.Jul.Aug.Sep.Oct.Nov.Dec."


@dataclass
class FontSpec:
    name: str  # file stem; the C++ symbol is derived from it
    weight: int  # variable-font wght axis, 100-900
    size: int  # em size in pixels
    chars: str
    purpose: str


# Sizes follow the Raspberry Pi version's layout as a starting point.
FONTS = [
    FontSpec("noto_sans_tc_bold_30", 700, 30, CALENDAR_TEXT, "event list"),
    FontSpec("noto_sans_tc_bold_40", 700, 40, DIGITS + WEEKDAYS, "year, weekday"),
    FontSpec("noto_sans_tc_medium_50", 500, 50, MONTH_ABBREVIATIONS, "month"),
    FontSpec("noto_sans_tc_regular_100", 400, 100, DIGITS, "day of month"),
]


@dataclass
class Glyph:
    codepoint: int
    width: int
    height: int
    bearing_x: int
    bearing_y: int
    advance: int
    rows: bytes  # packed 1bpp, MSB first, each row padded to a byte


def fetch(url, path):
    if not path.exists():
        CACHE.mkdir(exist_ok=True)
        print(f"downloading {url}")
        urllib.request.urlretrieve(url, path)
    return path


def fetch_font():
    path = fetch(FONT_URL, CACHE / "NotoSansTC[wght].ttf")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if FONT_SHA256 and digest != FONT_SHA256:
        raise SystemExit(f"{path.name} changed upstream (sha256 {digest}); review, then update FONT_SHA256")
    return path


def symbol_name(stem):
    return "k" + "".join(part.capitalize() for part in stem.split("_"))


def rasterize(font, ch):
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
    advance = round(font.getlength(ch))
    if x1 <= x0 or y1 <= y0:
        return Glyph(ord(ch), 0, 0, 0, 0, advance, b"")

    img = Image.new("L", (x1 - x0, y1 - y0), 0)
    ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
    ink = img.point(lambda v: 255 if v >= INK_THRESHOLD else 0)

    box = ink.getbbox()  # trim rows/columns that thresholding emptied
    if box is None:
        return Glyph(ord(ch), 0, 0, 0, 0, advance, b"")
    ink = ink.crop(box)
    width, height = ink.size
    row_bytes = (width + 7) // 8
    rows = bytearray()
    pixels = ink.load()
    for y in range(height):
        row = bytearray(row_bytes)
        for x in range(width):
            if pixels[x, y]:
                row[x // 8] |= 0x80 >> (x % 8)
        rows += row
    return Glyph(ord(ch), width, height, x0 + box[0], -(y0 + box[1]), advance, bytes(rows))


def bake(spec, font_path, covered):
    font = ImageFont.truetype(str(font_path), spec.size)
    font.set_variation_by_axes([spec.weight])
    ascent, descent = font.getmetrics()

    chars = sorted(set(spec.chars), key=ord)
    missing = [c for c in chars if ord(c) not in covered]
    glyphs = [rasterize(font, c) for c in chars if ord(c) in covered]
    return glyphs, ascent, ascent + descent, missing


def emit_cpp(spec, glyphs, ascent, line_height):
    symbol = symbol_name(spec.name)
    lines = [
        f"// Generated by tools/font_subset/font_subset.py -- do not edit.",
        f"// Noto Sans TC, weight {spec.weight}, {spec.size}px: {spec.purpose}.",
        "// SIL Open Font License 1.1, see LICENSE-OFL.txt.",
        "",
        '#include "fonts.hpp"',
        "",
        "namespace {",
        "",
        "constexpr uint8_t kBitmaps[] = {",
    ]
    offsets = []
    blob = bytearray()
    for g in glyphs:
        offsets.append(len(blob))
        blob += g.rows
    for i in range(0, len(blob), 20):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in blob[i : i + 20]) + ",")
    if not blob:
        lines.append("    0x00,")
    lines += ["};", "", "constexpr Glyph kGlyphs[] = {"]
    for g, offset in zip(glyphs, offsets):
        label = chr(g.codepoint) if chr(g.codepoint).isprintable() and chr(g.codepoint) != "\\" else ""
        lines.append(
            f"    {{0x{g.codepoint:04x}, {offset}, {g.width}, {g.height}, {g.bearing_x}, {g.bearing_y}, {g.advance}}},"
            f"  // U+{g.codepoint:04X} {label}".rstrip()
        )
    lines += [
        "};",
        "",
        "}  // namespace",
        "",
        f"constexpr Font {symbol}{{kGlyphs, kBitmaps, {ascent}, {line_height}}};",
        "",
    ]
    (OUT_DIR / f"{spec.name}.cpp").write_text("\n".join(lines), encoding="utf-8")
    return len(blob) + len(glyphs) * 20  # sizeof(Glyph) == 20


def emit_header_and_cmake(specs):
    header = [
        "#pragma once",
        "",
        "// Generated by tools/font_subset/font_subset.py -- do not edit.",
        "// Noto Sans TC baked to 1bpp bitmaps; SIL Open Font License 1.1, see LICENSE-OFL.txt.",
        "",
        '#include "font.hpp"',
        "",
    ]
    for spec in specs:
        header.append(f"extern const Font {symbol_name(spec.name)};  // {spec.purpose}")
    (OUT_DIR / "fonts.hpp").write_text("\n".join(header) + "\n", encoding="utf-8")

    srcs = " ".join(f'"{spec.name}.cpp"' for spec in specs)
    cmake = f'idf_component_register(SRCS {srcs}\n                    INCLUDE_DIRS "."\n                    REQUIRES canvas)\n'
    (OUT_DIR / "CMakeLists.txt").write_text(cmake, encoding="utf-8")


def main():
    font_path = fetch_font()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    license_text = fetch(LICENSE_URL, CACHE / "OFL.txt").read_text(encoding="utf-8")
    (OUT_DIR / "LICENSE-OFL.txt").write_text(license_text, encoding="utf-8")

    covered = set(TTFont(str(font_path)).getBestCmap().keys())
    total = 0
    for spec in FONTS:
        glyphs, ascent, line_height, missing = bake(spec, font_path, covered)
        size = emit_cpp(spec, glyphs, ascent, line_height)
        total += size
        note = f", {len(missing)} not in the font: {''.join(missing)[:40]!r}" if missing else ""
        print(f"{spec.name}: {len(glyphs)} glyphs, {size / 1024:.0f} KB{note}")
    emit_header_and_cmake(FONTS)
    print(f"total {total / 1024:.0f} KB of flash")


if __name__ == "__main__":
    main()
