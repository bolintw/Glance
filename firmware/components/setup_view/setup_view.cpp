#include "setup_view.hpp"

#include <iterator>
#include <string>

#include "fonts.hpp"
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
constexpr int kTextX = 460;
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

}  // namespace

void render(Canvas& canvas, std::string_view qrPayload, std::string_view ssid, std::string_view password,
            std::string_view url) {
    canvas.fill(kBackground);
    drawQr(canvas, qrPayload);

    const Font& font = kNotoSansTcBold30;
    const std::string lines[] = {
        "設定模式",
        "",
        "1. 先複製好 ICS 網址",
        "2. 相機掃描 QR code",
        "3. 稍等設定頁跳出",
        "",
        "沒有跳出來的話請開啟",
        std::string(url),
        "",
        "網路：" + std::string(ssid),
        "密碼：" + std::string(password),
    };
    int y = (static_cast<int>(canvas.frame().height) - static_cast<int>(std::size(lines)) * kLinePitch) / 2;
    for (const std::string& line : lines) {
        canvas.drawText(kTextX, y, line, font, kInk);
        y += kLinePitch;
    }
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
