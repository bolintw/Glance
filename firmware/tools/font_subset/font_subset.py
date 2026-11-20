"""Bakes Noto Sans TC into the bitmap fonts the firmware draws with.

Latin letters Noto Sans TC lacks (Polish, Czech, Turkish... ł č ş) come from
Source Sans 3, the family Noto Sans CJK's own Latin glyphs were drawn from,
scaled so its letters match the CJK font's. The event list also carries a
few hundred emoji from the black-and-white Noto Emoji: the ones calendars
use and the most used ones overall, not all ~1,400 (those would take a
third of the app's free flash).

Each entry in FONTS becomes components/fonts/<name>.cpp: a flash-resident
Glyph table plus 1bpp bitmaps for exactly the characters that font needs.
Big sizes (the day number, the weekday) only carry the handful of glyphs
they show, so only the event-list font pays for the full character set.

    python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
    .venv/bin/python font_subset.py

The fonts are downloaded into .cache/ on first run (gitignored); their SHA-256
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

LATIN_FONT_URL = "https://github.com/google/fonts/raw/main/ofl/sourcesans3/SourceSans3%5Bwght%5D.ttf"
LATIN_FONT_SHA256 = "042fe2cc0b933e328410d7acbd0aa6a1873dca5aef81875f4bc214b08825c7b9"
LATIN_LICENSE_URL = "https://raw.githubusercontent.com/google/fonts/main/ofl/sourcesans3/OFL.txt"
# Source Sans 3 at 34px has the same x-height and widths as Noto Sans TC's
# Latin at 30px.
LATIN_SCALE = 34 / 30

EMOJI_FONT_URL = "https://github.com/google/fonts/raw/main/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf"
EMOJI_FONT_SHA256 = "de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551"
EMOJI_LICENSE_URL = "https://raw.githubusercontent.com/google/fonts/main/ofl/notoemoji/OFL.txt"
# Noto Emoji at 27px stands as tall as Noto Sans TC's hanzi at 30px.
EMOJI_SCALE = 27 / 30

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
# Latin-1 Supplement and Latin Extended-A: accented letters (Café, Zoë,
# São Paulo, Kraków), ß, ¿¡, and the € sign for European event titles.
LATIN = "".join(chr(c) for c in range(0xA1, 0x180) if c != 0xAD) + "€"

CALENDAR_TEXT = ASCII + BIG5_SYMBOLS + BIG5_COMMON_HANZI + EXTRA_PUNCTUATION + LATIN

# Emoji people put in calendar titles: occasions, travel, health, sports,
# work and school, home, food, music and films, weather.
CALENDAR_EMOJI = (
    "🎂🎉🎊🎁🎈🥳🍰🎄🎃🧧🏮🎆🎇💍💒🎓🏆🥇✈🛫🛬🚆🚄🚌🚗🚕🚲🛵🚢⛴"
    "🗺🧳🏖🏝⛰🏕🗼🏨🚇🚉⛽🅿🏥💊💉🦷🩺🩸🧘💆💇⚽🏀🏸🎾🏐⚾🏈🏓🏊"
    "🏃🚴🧗⛳🏋🥊⛷🏄🤸🎳🏌💼📅📆🗓📌📝📚✏💻🖥📞☎📧📮📦📊📈🏢🏫"
    "🎒🧑🔔⏰⌛💰💳🧾🏦🏠🧹🧺🛒🔧🔨🪴🌱🐶🐱🐾🚿🗑🔑🍽🍴☕🍵🍺🍻🍷"
    "🥂🍕🍔🍜🍣🍱🍲🥘🍝🥗🍳🥐🍦🧋🍙🍗🎵🎶🎤🎧🎸🎹🎻🥁🎬🎥🎮🎨🎭🎟"
    "🎫📸📷📺🍿📖🎲☀🌧☔⛅❄🌙⭐🌈🔥💧👶👪❤💕✅❗⚠📍🔴🟢🔵⭕❌"
    "✨💡🙏👑🌸🌹💐"
)
# The 300 most used emoji, from Unicode's frequency ranking
# (https://www.unicode.org/emoji/frequency.html); about 94% of all emoji use
# by its estimate. A sequence (👩‍🚀, 👍🏽) counts as its first emoji, which is
# all the firmware draws of it.
FREQUENT_EMOJI = (
    "😂❤😍🤣😊🙏💕😭😘👍😅👏😁♥🔥💔💖💙😢🤔😆🙄💪😉☺👌🤗💜😔😎"
    "😇🌹🤦🎉‼💞✌✨🤷😱😌🌸🙌😋💗💚😏💛🙂💓🤩😄😀🖤😃💯🙈👇🎶😒"
    "🤭❣❗😜💋👀😪😑💥🙋😞😩😡🤪👊☀😥🤤👉💃😳✋😚😝😴🌟😬🙃🍀🌷"
    "😻😓⭐✅🌈😈🤘💦✔😣🏃💐☹🎊💘😠☝😕🌺🎂🌻😐🖕💝🙊😹🗣💫💀👑"
    "🎵🤞😛🔴😤🌼😫⚽🤙☕🏆🧡🎁⚡🌞🎈❌✊👋😲🌿🤫👈😮🙆🍻🍃🐶💁😰"
    "🤨😶🤝🚶💰🍓💢🇺🤟🙁🚨💨🤬✈🎀🍺🤓😙💟🌱😖👶▶➡❓💎💸⬇😨🌚"
    "🦋😷🕺⚠🙅😟😵👎🤲🤠🤧📌🔵💅🧐🐾🍒😗🤑🚀🌊🤯🐷☎💧😯💆👆🎤🙇"
    "🍑❄🌴🇧💣🐸💌📍🥀🤢👅💡💩⁉👐📸👻🤐🤮🎼✍🚩🍎🍊👼💍📣🥂⤵📱"
    "☔🌙🍾🎧🍁⭕🏀☠⚫🖐😧🎯📲☘👁🍷👄🐟🍰💤🕊📺💭🐱🐝🇲🧚🔝📢📷"
    "🐕🎸🔫🤚🍭🍆💉🌎😦🌀👿☑🎥🌧👽🍋🤒🤡🍫📚🏁🤕🦄🍅🚗🚫💵⚾🔪🔔"
)
# Without the regional indicators: a flag is two of them, and half a flag
# reads worse than none (the firmware drops what the font lacks).
EMOJI = "".join(c for c in CALENDAR_EMOJI + FREQUENT_EMOJI if not 0x1F1E6 <= ord(c) <= 0x1F1FF)
DIGITS = "0123456789"
WEEKDAYS = "一二三四五六日"
MONTH_ABBREVIATIONS = "Jan.Feb.Mar.Apr.May.Jun.Jul.Aug.Sep.Oct.Nov.Dec."
WEATHER_TEXT = DIGITS + "°-% "  # "24° - 28°", "50%"
EVENT_TIME_TEXT = DIGITS + ":"  # "14:30"
# The 30px font only writes fixed text now: the setup screens' network
# name, password and address (ASCII), privacy mode's month, "請充電".
FIXED_TEXT = ASCII + "請充電"


@dataclass
class FontSpec:
    name: str  # file stem; the C++ symbol is derived from it
    weight: int  # variable-font wght axis, 100-900
    size: int  # em size in pixels
    chars: str
    purpose: str
    latin_fallback: bool = False  # fill letters Noto Sans TC lacks from Source Sans 3
    emoji: str = ""  # drawn from Noto Emoji, unless `chars` already has them


# Sizes follow the Raspberry Pi version's layout as a starting point.
FONTS = [
    FontSpec("noto_sans_tc_bold_26", 700, 26, CALENDAR_TEXT, "event list", latin_fallback=True, emoji=EMOJI),
    FontSpec("noto_sans_tc_bold_16", 700, 16, EVENT_TIME_TEXT, "event start and end times"),
    FontSpec("noto_sans_tc_bold_30", 700, 30, FIXED_TEXT, "setup screens, privacy label month, charge reminder"),
    FontSpec("noto_sans_tc_bold_40", 700, 40, DIGITS + WEEKDAYS, "year, weekday"),
    FontSpec("noto_sans_tc_medium_50", 500, 50, MONTH_ABBREVIATIONS, "month"),
    FontSpec("noto_sans_tc_regular_100", 400, 100, DIGITS, "day of month"),
    FontSpec("noto_sans_tc_bold_20", 700, 20, WEATHER_TEXT, "temperature, chance of rain"),
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


def fetch_font(url, name, sha256):
    path = fetch(url, CACHE / name)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if sha256 and digest != sha256:
        raise SystemExit(f"{path.name} changed upstream (sha256 {digest}); review, then update its pinned SHA-256")
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


def bake(spec, sources):
    (font_path, covered), (latin_path, latin_covered), (emoji_path, emoji_covered) = sources
    font = ImageFont.truetype(str(font_path), spec.size)
    font.set_variation_by_axes([spec.weight])
    ascent, descent = font.getmetrics()
    latin = None
    if spec.latin_fallback:
        latin = ImageFont.truetype(str(latin_path), round(spec.size * LATIN_SCALE))
        latin.set_variation_by_axes([spec.weight])
    emoji = None
    if spec.emoji:
        emoji = ImageFont.truetype(str(emoji_path), round(spec.size * EMOJI_SCALE))
        emoji.set_variation_by_axes([spec.weight])

    text = set(spec.chars)
    glyphs, missing = [], []
    for c in sorted(text | set(spec.emoji), key=ord):
        if c in text and ord(c) in covered:
            glyphs.append(rasterize(font, c))
        elif c in text and latin and ord(c) in latin_covered:
            glyphs.append(rasterize(latin, c))
        elif emoji and ord(c) in emoji_covered:
            glyphs.append(rasterize(emoji, c))
        else:
            missing.append(c)
    return glyphs, ascent, ascent + descent, missing


def emit_cpp(spec, glyphs, ascent, line_height):
    symbol = symbol_name(spec.name)
    lines = [
        f"// Generated by tools/font_subset/font_subset.py -- do not edit.",
        f"// Noto Sans TC, weight {spec.weight}, {spec.size}px: {spec.purpose}."
        + (" Latin letters it lacks from Source Sans 3." if spec.latin_fallback else "")
        + (" Emoji from Noto Emoji." if spec.emoji else ""),
        "// SIL Open Font License 1.1, see LICENSE-OFL.txt"
        + (", LICENSE-OFL-SourceSans3.txt and LICENSE-OFL-NotoEmoji.txt." if spec.emoji else "."),
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
        "// Noto Sans TC (plus Source Sans 3 for Latin letters it lacks, and Noto Emoji) baked to",
        "// 1bpp bitmaps; SIL Open Font License 1.1, see the LICENSE-OFL*.txt files.",
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
    font_path = fetch_font(FONT_URL, "NotoSansTC[wght].ttf", FONT_SHA256)
    latin_path = fetch_font(LATIN_FONT_URL, "SourceSans3[wght].ttf", LATIN_FONT_SHA256)
    emoji_path = fetch_font(EMOJI_FONT_URL, "NotoEmoji[wght].ttf", EMOJI_FONT_SHA256)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for url, cached, out in [
        (LICENSE_URL, "OFL.txt", "LICENSE-OFL.txt"),
        (LATIN_LICENSE_URL, "OFL-SourceSans3.txt", "LICENSE-OFL-SourceSans3.txt"),
        (EMOJI_LICENSE_URL, "OFL-NotoEmoji.txt", "LICENSE-OFL-NotoEmoji.txt"),
    ]:
        license_text = fetch(url, CACHE / cached).read_text(encoding="utf-8")
        (OUT_DIR / out).write_text(license_text, encoding="utf-8")

    sources = [(path, set(TTFont(str(path)).getBestCmap().keys())) for path in (font_path, latin_path, emoji_path)]
    total = 0
    for spec in FONTS:
        glyphs, ascent, line_height, missing = bake(spec, sources)
        size = emit_cpp(spec, glyphs, ascent, line_height)
        total += size
        note = f", {len(missing)} not in the font: {''.join(missing)[:40]!r}" if missing else ""
        print(f"{spec.name}: {len(glyphs)} glyphs, {size / 1024:.0f} KB{note}")
    emit_header_and_cmake(FONTS)
    print(f"total {total / 1024:.0f} KB of flash")


if __name__ == "__main__":
    main()
