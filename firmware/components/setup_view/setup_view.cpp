#include "setup_view.hpp"

#include <iterator>
#include <string>

#include "fonts.hpp"
#include "icons.hpp"
#include "qrcodegen.h"

namespace setup_view {
namespace {

constexpr Color kBackground = Color::black;
constexpr Color kInk = Color::white;

// The QR code fills a square on the left. It's always dark modules on light
// with a 4-module quiet zone, whatever the rest of the screen does: not
// every phone camera reads inverted codes.
constexpr int kQrX = 40;
constexpr int kQrMaxSize = 400;
constexpr int kQuietZone = 4;
constexpr int kLinePitch = 42;

void drawQr(Canvas& canvas, std::string_view payload) {
    std::string text(payload);
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(text.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, 10,
                              qrcodegen_Mask_AUTO, true)) {
        return;  // payload too long for version 10; can't happen for an SSID + password
    }
    int modules = qrcodegen_getSize(qr);
    int scale = kQrMaxSize / (modules + 2 * kQuietZone);
    int size = (modules + 2 * kQuietZone) * scale;
    int top = (static_cast<int>(canvas.frame().height) - size) / 2;
    canvas.fillRect(kQrX, top, size, size, Color::white);
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (qrcodegen_getModule(qr, x, y)) {
                canvas.fillRect(kQrX + (kQuietZone + x) * scale, top + (kQuietZone + y) * scale, scale, scale,
                                Color::black);
            }
        }
    }
}

constexpr int kWidth = 800;
constexpr int kHeight = 480;
constexpr int kRightCentre = 620;  // the column right of the QR code
constexpr int kDotRadius = 8;

// Draws an icon from kSetupIcons with its box's bottom-left at (x, bottom).
void drawIcon(Canvas& canvas, int x, int bottom, std::string_view icon) {
    canvas.drawText(x, bottom - kSetupIcons.ascent, icon, kSetupIcons, kInk);
}

int iconWidth(std::string_view icon) { return measureText(icon, kSetupIcons); }

// The icon centred on `centreX`, its box's bottom at `bottom`.
void drawIconCentred(Canvas& canvas, int centreX, int bottom, std::string_view icon) {
    drawIcon(canvas, centreX - iconWidth(icon) / 2, bottom, icon);
}

// Which of the two setup steps this is: a filled dot for it, a ring for
// the other.
void drawStepDots(Canvas& canvas, int centreX, int y, int step) {
    for (int i = 0; i < 2; i++) {
        int cx = centreX + (i == 0 ? -20 : 20);
        for (int dy = -kDotRadius; dy <= kDotRadius; dy++) {
            for (int dx = -kDotRadius; dx <= kDotRadius; dx++) {
                int d2 = dx * dx + dy * dy;
                bool inside = d2 <= kDotRadius * kDotRadius;
                bool ring = inside && d2 >= (kDotRadius - 3) * (kDotRadius - 3);
                if (i + 1 == step ? inside : ring) {
                    canvas.setPixel(cx + dx, y + dy, kInk);
                }
            }
        }
    }
}

// A small icon and a value after it, e.g. the password after a key; `y`
// is the line's top, as for drawText.
void drawValue(Canvas& canvas, int x, int y, std::string_view icon, std::string_view value) {
    const Font& font = kNotoSansTcBold30;
    // Latin text sits on the baseline with room below for descenders; the
    // icon's bottom lines up a little under it.
    int baseline = y + font.ascent;
    drawIcon(canvas, x, baseline + 4, icon);
    canvas.drawText(x + iconWidth(icon) + 12, y, value, font, kInk);
}

}  // namespace

void render(Canvas& canvas, std::string_view qrPayload, std::string_view ssid, std::string_view password,
            std::string_view url) {
    canvas.fill(kBackground);
    drawQr(canvas, qrPayload);
    drawIconCentred(canvas, kRightCentre, 106, icons::kSetupWifi);
    drawIconCentred(canvas, kRightCentre + 10, 262, icons::kSetupScanHand);  // scan it with the phone
    drawStepDots(canvas, kRightCentre, 282, 1);
    // For when the QR code doesn't work or the page doesn't pop up.
    int x = kRightCentre - 150;
    drawValue(canvas, x, 306, icons::kSetupSmallWifi, ssid);
    drawValue(canvas, x, 356, icons::kSetupSmallKey, password);
    drawValue(canvas, x, 406, icons::kSetupSmallGlobe, url);
}

void renderLan(Canvas& canvas, std::string_view qrPayload) {
    canvas.fill(kBackground);
    drawQr(canvas, qrPayload);
    drawIconCentred(canvas, kRightCentre, 300, icons::kSetupGear);
    drawStepDots(canvas, kRightCentre, 340, 2);
}

void renderPhoneJoined(Canvas& canvas, std::string_view url) {
    canvas.fill(kBackground);
    // The hands come in from the bottom edge.
    drawIconCentred(canvas, kWidth / 2, kHeight, icons::kSetupPhoneHand);
    if (!url.empty()) {
        const Font& font = kNotoSansTcBold30;
        int width = iconWidth(icons::kSetupSmallGlobe) + 12 + measureText(url, font);
        drawValue(canvas, (kWidth - width) / 2, 22, icons::kSetupSmallGlobe, url);
    }
}

void renderIntruder(Canvas& canvas) {
    canvas.fill(kBackground);
    // Two phones on one crossed-out WiFi, and the way out in the corner: a
    // finger on the frame's button (it comes in from the bottom edge).
    drawIconCentred(canvas, kWidth / 2, kHeight, icons::kSetupIntruder);
}

void renderShip(Canvas& canvas) {
    canvas.fill(kBackground);
    drawIconCentred(canvas, kWidth / 2, kHeight, icons::kSetupShip);  // hands in from the bottom edge
}

void renderNotice(Canvas& canvas, std::string_view title, std::span<const std::string_view> lines) {
    canvas.fill(kBackground);
    const Font& font = kNotoSansTcBold30;
    int width = static_cast<int>(canvas.frame().width);
    int total = static_cast<int>(lines.size() + 2) * kLinePitch;  // title, a gap, the lines
    int y = (static_cast<int>(canvas.frame().height) - total) / 2;
    canvas.drawText((width - measureText(title, font)) / 2, y, title, font, kInk);
    y += 2 * kLinePitch;
    for (std::string_view line : lines) {
        canvas.drawText((width - measureText(line, font)) / 2, y, line, font, kInk);
        y += kLinePitch;
    }
}

}  // namespace setup_view
