#include "canvas.hpp"

#include <algorithm>

namespace {

constexpr uint32_t kReplacementChar = 0xFFFD;

// Decodes the code point at text[pos] and advances pos past it. Malformed
// bytes decode to U+FFFD one byte at a time, so bad input can't stall or
// overrun.
uint32_t nextCodepoint(std::string_view text, size_t& pos) {
    auto byteAt = [&](size_t i) { return static_cast<uint8_t>(text[i]); };
    uint8_t lead = byteAt(pos);
    size_t length;
    uint32_t cp;
    if (lead < 0x80) {
        pos++;
        return lead;
    } else if ((lead & 0xE0) == 0xC0) {
        length = 2;
        cp = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        cp = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        cp = lead & 0x07;
    } else {
        pos++;
        return kReplacementChar;
    }
    if (pos + length > text.size()) {
        pos++;
        return kReplacementChar;
    }
    for (size_t i = 1; i < length; i++) {
        uint8_t b = byteAt(pos + i);
        if ((b & 0xC0) != 0x80) {
            pos++;
            return kReplacementChar;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    pos += length;
    return cp;
}

// Placeholder for characters the font doesn't cover.
struct MissingBox {
    int width;
    int height;
    int advance;
};

MissingBox missingBox(const Font& font) {
    int width = std::max(3, font.ascent / 2);
    return {width, std::max(3, font.ascent * 3 / 4), width + 2};
}

int advanceOf(uint32_t cp, const Font& font) {
    const Glyph* glyph = font.find(cp);
    return glyph ? glyph->advance : missingBox(font).advance;
}

}  // namespace

const Glyph* Font::find(uint32_t codepoint) const {
    auto it = std::lower_bound(glyphs.begin(), glyphs.end(), codepoint,
                               [](const Glyph& g, uint32_t cp) { return g.codepoint < cp; });
    return (it != glyphs.end() && it->codepoint == codepoint) ? &*it : nullptr;
}

Canvas::Canvas(std::span<uint8_t> framebuffer, FrameSize frame) : framebuffer_(framebuffer), frame_(frame) {}

void Canvas::fill(Color color) { std::fill(framebuffer_.begin(), framebuffer_.end(), color == Color::white ? 0xFF : 0x00); }

void Canvas::setPixel(int x, int y, Color color) {
    if (x < 0 || y < 0 || static_cast<size_t>(x) >= frame_.width || static_cast<size_t>(y) >= frame_.height) {
        return;
    }
    size_t index = static_cast<size_t>(y) * frame_.bytesPerRow() + static_cast<size_t>(x) / 8;
    auto mask = static_cast<uint8_t>(0x80 >> (x % 8));
    if (color == Color::white) {
        framebuffer_[index] |= mask;
    } else {
        framebuffer_[index] &= static_cast<uint8_t>(~mask);
    }
}

void Canvas::fillRect(int x, int y, int width, int height, Color color) {
    int x0 = std::max(x, 0);
    int y0 = std::max(y, 0);
    int x1 = std::min(x + width, static_cast<int>(frame_.width));
    int y1 = std::min(y + height, static_cast<int>(frame_.height));
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            setPixel(px, py, color);
        }
    }
}

void Canvas::drawBitmap(int x, int y, const Bitmap& bitmap) {
    if (bitmap.width <= 0 || bitmap.height <= 0 || bitmap.bits.size() < Bitmap::sizeFor(bitmap.width, bitmap.height)) {
        return;
    }
    const size_t stride = Bitmap::rowBytes(bitmap.width);
    for (int row = 0; row < bitmap.height; row++) {
        const uint8_t* bits = bitmap.bits.data() + static_cast<size_t>(row) * stride;
        for (int col = 0; col < bitmap.width; col++) {
            bool white = bits[col / 8] & (0x80 >> (col % 8));
            setPixel(x + col, y + row, white ? Color::white : Color::black);
        }
    }
}

void Canvas::roundedRect(int x, int y, int width, int height, int radius, int thickness, Color color) {
    radius = std::clamp(radius, 0, std::min(width, height) / 2);
    thickness = std::clamp(thickness, 1, std::min(width, height) / 2);

    fillRect(x + radius, y, width - 2 * radius, thickness, color);
    fillRect(x + radius, y + height - thickness, width - 2 * radius, thickness, color);
    fillRect(x, y + radius, thickness, height - 2 * radius, color);
    fillRect(x + width - thickness, y + radius, thickness, height - 2 * radius, color);

    // Corners: fill every pixel whose centre lies in the ring between the
    // outer and inner radius. Works in doubled coordinates to stay integer,
    // and unlike stacking midpoint-circle arcs it leaves no gaps in thick
    // borders.
    const int outer = 4 * radius * radius;
    const int innerRadius = radius - thickness;
    const int inner = innerRadius > 0 ? 4 * innerRadius * innerRadius : -1;
    for (int i = 0; i < radius; i++) {
        for (int j = 0; j < radius; j++) {
            int dx = 2 * (radius - i) - 1;
            int dy = 2 * (radius - j) - 1;
            int d = dx * dx + dy * dy;
            if (d > outer || d <= inner) {
                continue;
            }
            setPixel(x + i, y + j, color);
            setPixel(x + width - 1 - i, y + j, color);
            setPixel(x + i, y + height - 1 - j, color);
            setPixel(x + width - 1 - i, y + height - 1 - j, color);
        }
    }
}

int Canvas::drawText(int x, int y, std::string_view utf8, const Font& font, Color color) {
    const int baseline = y + font.ascent;
    int pen = x;
    for (size_t pos = 0; pos < utf8.size();) {
        uint32_t cp = nextCodepoint(utf8, pos);
        const Glyph* glyph = font.find(cp);
        if (!glyph) {
            MissingBox box = missingBox(font);
            roundedRect(pen + 1, baseline - box.height, box.width, box.height, 0, 1, color);
            pen += box.advance;
            continue;
        }
        const int left = pen + glyph->bearingX;
        const int top = baseline - glyph->bearingY;
        const size_t rowBytes = (glyph->width + 7) / 8;
        const uint8_t* rows = font.bitmaps.data() + glyph->bitmapOffset;
        for (int row = 0; row < glyph->height; row++) {
            for (int col = 0; col < glyph->width; col++) {
                if (rows[row * rowBytes + col / 8] & (0x80 >> (col % 8))) {
                    setPixel(left + col, top + row, color);
                }
            }
        }
        pen += glyph->advance;
    }
    return pen - x;
}

int measureText(std::string_view utf8, const Font& font) {
    int width = 0;
    for (size_t pos = 0; pos < utf8.size();) {
        width += advanceOf(nextCodepoint(utf8, pos), font);
    }
    return width;
}

std::string ellipsize(std::string_view utf8, const Font& font, int maxWidth) {
    if (measureText(utf8, font) <= maxWidth) {
        return std::string(utf8);
    }
    constexpr std::string_view kEllipsis = "...";
    const int budget = maxWidth - measureText(kEllipsis, font);
    int width = 0;
    size_t keep = 0;
    for (size_t pos = 0; pos < utf8.size();) {
        size_t next = pos;
        width += advanceOf(nextCodepoint(utf8, next), font);
        if (width > budget) {
            break;
        }
        pos = next;
        keep = pos;
    }
    // "重複 ..." reads worse than "重複...".
    while (keep > 0 && utf8[keep - 1] == ' ') {
        keep--;
    }
    return std::string(utf8.substr(0, keep)) + std::string(kEllipsis);
}
