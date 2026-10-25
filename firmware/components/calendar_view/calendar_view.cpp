#include "calendar_view.hpp"

#include <cstdio>
#include <string>
#include <utility>

#include "fonts.hpp"
#include "icons.hpp"
#include "photos.hpp"
#include "ics_datetime.hpp"
#include "qrcodegen.h"

namespace calendar_view {

namespace {

// Layout carried over from the Raspberry Pi version (ePaper-Google-Calendar,
// drawing.py + its base BMP): white on black, date header on top, a rounded
// box with two columns of five events.
//
// The header is placed by baseline rather than by the Pi's top-left text
// coordinates: those were tuned for Helvetica/Avant Garde, and Noto Sans
// TC's ascent is much taller (116px at 100px vs. a 75px digit height), so
// the same coordinates pushed the day number down into the event box. In the
// Pi layout the month, day and weekday all sit on the line where the header
// dividers end.
// The dark theme's; the light theme swaps them (see colorsFor).
constexpr Color kBackground = Color::black;
constexpr Color kInk = Color::white;

constexpr int kYearX = 50, kYearBaseline = 58;
constexpr int kHeaderBaseline = 126;
constexpr int kMonthRight = 350;
constexpr int kDayCenter = 425;
constexpr int kWeekdayX = 500;
// CJK glyphs hang ~4px below the Latin baseline at 40px; lift the weekday so
// its bottom lines up with the digits'.
constexpr int kWeekdayBaseline = kHeaderBaseline - 4;
constexpr int kHeaderDividerX[] = {357, 494};
constexpr int kHeaderDividerTop = 69, kHeaderDividerBottom = 126;

// Weather, top right. The icon box and raindrop are where the Pi's
// background images had them (see tools/icons/); the text sits on the
// raindrop's baseline.
constexpr int kWeatherIconX = 615, kWeatherIconBaseline = 110;
constexpr int kRaindropX = 716, kRaindropBaseline = 130;
constexpr int kWeatherTextBaseline = 127;
constexpr int kTemperatureCenter = 668;
constexpr int kRainChanceX = 735;

// Status icons (24x24), top right: right-aligned with the weather text and
// above the weather icon, which starts at y=37. The battery goes rightmost,
// like on a phone.
constexpr int kStatusIconRight = 777, kStatusIconBaseline = 32, kStatusIconSize = 24, kStatusIconGap = 8;

constexpr int kBoxX = 32, kBoxY = 160, kBoxWidth = 738, kBoxHeight = 288, kBoxRadius = 15;
constexpr int kColumnDividerX = 401;
constexpr int kColumnTextX[] = {55, 420};
constexpr int kColumnRuleX[] = {50, 419};
constexpr int kRuleWidth = 333;
constexpr int kFirstRowY = 165;
constexpr int kRowPitch = 48;
constexpr int kRowsPerColumn = 5;
constexpr int kMaxTextWidth = 331;
// The photo sits inside the box's 2px border with a 2px gap.
constexpr int kPhotoX = kBoxX + 4, kPhotoY = kBoxY + 4;
static_assert(photos::kWidth == kBoxWidth - 8 && photos::kHeight == kBoxHeight - 8);

// The corner QR code (a guest WiFi, a link...), in privacy mode, where the weather goes: a frame
// like the photo box's, right-aligned with it, and the code on white inside
// with a black gap. The status icons move left of the frame.
constexpr int kQrFrameSize = 144, kQrFrameY = 8;
constexpr int kQrFrameX = kBoxX + kBoxWidth - kQrFrameSize;
constexpr int kQrMaxSize = kQrFrameSize - 12;  // inside the 2px border and a 4px gap
constexpr int kQrQuietZone = 2;  // modules; the white around the code
constexpr int kQrStatusRight = kQrFrameX - 10;

constexpr const char* kMonths[] = {"Jan.", "Feb.", "Mar.", "Apr.", "May.", "Jun.",
                                   "Jul.", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
constexpr const char* kWeekdays[] = {"一", "二", "三", "四", "五", "六", "日"};  // Monday first

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

ics::Date localDate(int64_t unix, int32_t utcOffset) { return ics::civilFromDays(floorDiv(unix + utcOffset, 86400)); }

// The Pi version's row separators were grey; on a 1bpp panel a dotted line
// reads as the lighter rule.
void dottedRule(Canvas& canvas, int x, int y, int width, Color ink) {
    for (int i = 0; i < width; i += 2) {
        canvas.setPixel(x + i, y, ink);
    }
}

void drawOnBaseline(Canvas& canvas, int x, int baseline, std::string_view text, const Font& font, Color ink) {
    canvas.drawText(x, baseline - font.ascent, text, font, ink);
}

void drawHeader(Canvas& canvas, ics::Date today, Color ink) {
    char text[8];

    std::snprintf(text, sizeof(text), "%d", today.year);
    drawOnBaseline(canvas, kYearX, kYearBaseline, text, kNotoSansTcBold40, ink);

    const char* month = kMonths[today.month - 1];
    drawOnBaseline(canvas, kMonthRight - measureText(month, kNotoSansTcMedium50), kHeaderBaseline, month,
                   kNotoSansTcMedium50, ink);

    std::snprintf(text, sizeof(text), "%d", today.day);
    drawOnBaseline(canvas, kDayCenter - measureText(text, kNotoSansTcRegular100) / 2, kHeaderBaseline, text,
                   kNotoSansTcRegular100, ink);

    drawOnBaseline(canvas, kWeekdayX, kWeekdayBaseline, kWeekdays[ics::weekdayOf(today)], kNotoSansTcBold40, ink);

    for (int x : kHeaderDividerX) {
        canvas.fillRect(x, kHeaderDividerTop, 1, kHeaderDividerBottom - kHeaderDividerTop, ink);
    }
}

const char* iconFor(weather::Condition condition) {
    switch (condition) {
        case weather::Condition::sunny: return icons::kSunny;
        case weather::Condition::partlyCloudy: return icons::kPartlyCloudy;
        case weather::Condition::cloudy: return icons::kCloudy;
        case weather::Condition::windy: return icons::kWindy;
        case weather::Condition::rainy: return icons::kRainy;
    }
    return icons::kPartlyCloudy;
}

void drawWeather(Canvas& canvas, const weather::Forecast& forecast, Color ink) {
    drawOnBaseline(canvas, kWeatherIconX, kWeatherIconBaseline, iconFor(forecast.condition), kWeatherIcons, ink);

    char text[24];
    std::snprintf(text, sizeof(text), "%d° - %d°", forecast.minTemp, forecast.maxTemp);
    drawOnBaseline(canvas, kTemperatureCenter - measureText(text, kNotoSansTcBold20) / 2, kWeatherTextBaseline, text,
                   kNotoSansTcBold20, ink);

    drawOnBaseline(canvas, kRaindropX, kRaindropBaseline, icons::kRaindrop, kWeatherIcons, ink);
    std::snprintf(text, sizeof(text), "%d%%", forecast.rainChance);
    drawOnBaseline(canvas, kRainChanceX, kWeatherTextBaseline, text, kNotoSansTcBold20, ink);
}

void drawStatus(Canvas& canvas, Status status, Color ink, int right = kStatusIconRight) {
    for (auto [shown, icon] : {std::pair{status.lowBattery, icons::kBatteryLow}, std::pair{status.offline, icons::kWifiOff},
                               std::pair{status.calendarError, icons::kCalendarError},
                               std::pair{status.weatherError, icons::kWeatherError}}) {
        if (shown) {
            drawOnBaseline(canvas, right - kStatusIconSize, kStatusIconBaseline, icon, kWeatherIcons, ink);
            right -= kStatusIconSize + kStatusIconGap;
        }
    }
}

void drawCornerQr(Canvas& canvas, std::string_view payload, Color ink) {
    std::string text(payload);
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if (!qrcodegen_encodeText(text.c_str(), temp, qr, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 10,
                              qrcodegen_Mask_AUTO, true)) {
        return;  // too long for version 10; can't happen for an SSID + password
    }
    canvas.roundedRect(kQrFrameX, kQrFrameY, kQrFrameSize, kQrFrameSize, kBoxRadius, 2, ink);
    int modules = qrcodegen_getSize(qr);
    int scale = kQrMaxSize / (modules + 2 * kQrQuietZone);
    int size = (modules + 2 * kQrQuietZone) * scale;
    int left = kQrFrameX + (kQrFrameSize - size) / 2, top = kQrFrameY + (kQrFrameSize - size) / 2;
    canvas.fillRect(left, top, size, size, Color::white);
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (qrcodegen_getModule(qr, x, y)) {
                canvas.fillRect(left + (kQrQuietZone + x) * scale, top + (kQrQuietZone + y) * scale, scale,
                                scale, Color::black);
            }
        }
    }
}

void drawBox(Canvas& canvas, Color ink) { canvas.roundedRect(kBoxX, kBoxY, kBoxWidth, kBoxHeight, kBoxRadius, 2, ink); }

void drawEventBox(Canvas& canvas, int32_t utcOffset, std::span<const ics::Occurrence> events, Color ink) {
    drawBox(canvas, ink);
    canvas.fillRect(kColumnDividerX, kBoxY, 2, kBoxHeight, ink);
    for (int column = 0; column < 2; column++) {
        for (int row = 1; row <= kRowsPerColumn; row++) {  // a rule under every row
            dottedRule(canvas, kColumnRuleX[column], kFirstRowY + row * kRowPitch - 5, kRuleWidth, ink);
        }
    }

    if (events.empty()) {
        canvas.drawText(kColumnTextX[0], kFirstRowY, "沒有接下來的行程", kNotoSansTcBold30, ink);
        return;
    }
    for (size_t i = 0; i < events.size() && i < 2 * kRowsPerColumn; i++) {
        const ics::Occurrence& event = events[i];
        ics::Date date = localDate(event.start, utcOffset);
        char prefix[16];
        std::snprintf(prefix, sizeof(prefix), "%d/%d : ", date.month, date.day);
        std::string line = std::string(prefix) + (event.summary.empty() ? "(無標題)" : event.summary);

        int column = static_cast<int>(i) / kRowsPerColumn;
        int row = static_cast<int>(i) % kRowsPerColumn;
        canvas.drawText(kColumnTextX[column], kFirstRowY + row * kRowPitch,
                        ellipsize(line, kNotoSansTcBold30, kMaxTextWidth), kNotoSansTcBold30, ink);
    }
}

struct Colors {
    Color background;
    Color ink;
};

Colors colorsFor(Theme theme) {
    return theme == Theme::light ? Colors{kInk, kBackground} : Colors{kBackground, kInk};
}

}  // namespace

bool isDarkPhoto(const Bitmap& photo) {
    size_t white = 0;
    const size_t rowBytes = Bitmap::rowBytes(photo.width);
    for (int y = 0; y < photo.height; y++) {
        for (int x = 0; x < photo.width; x++) {
            white += (photo.bits[y * rowBytes + x / 8] >> (7 - x % 8)) & 1;
        }
    }
    return white * 2 < static_cast<size_t>(photo.width) * photo.height;
}

void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
            const std::optional<weather::Forecast>& forecast, Status status, Theme theme) {
    const auto [background, ink] = colorsFor(theme);
    canvas.fill(background);
    drawHeader(canvas, localDate(now, utcOffset), ink);
    if (forecast) {
        drawWeather(canvas, *forecast, ink);
    }
    drawStatus(canvas, status, ink);
    drawEventBox(canvas, utcOffset, events, ink);
}

void renderPrivate(Canvas& canvas, std::optional<int64_t> now, int32_t utcOffset,
                   const std::optional<weather::Forecast>& forecast, const Bitmap& photo, Status status,
                   std::string_view cornerQr, Theme theme) {
    const auto [background, ink] = colorsFor(theme);
    canvas.fill(background);
    if (now) {
        drawHeader(canvas, localDate(*now, utcOffset), ink);
    }
    if (!cornerQr.empty()) {
        drawCornerQr(canvas, cornerQr, ink);
        drawStatus(canvas, status, ink, kQrStatusRight);
    } else {
        if (forecast) {
            drawWeather(canvas, *forecast, ink);
        }
        drawStatus(canvas, status, ink);
    }
    drawBox(canvas, ink);
    canvas.drawBitmap(kPhotoX, kPhotoY, photo);
}

GrayOverlay photoOverlay(const GrayBitmap& photo) { return {kPhotoX, kPhotoY, photo}; }

void renderChargeReminder(Canvas& canvas) {
    canvas.fill(kBackground);
    constexpr int kWidth = 220, kHeight = 110, kStroke = 8, kTerminal = 18;
    int x = (800 - kWidth - kTerminal) / 2, y = 150;
    canvas.roundedRect(x, y, kWidth, kHeight, 14, kStroke, kInk);
    canvas.fillRect(x + kWidth + 4, y + kHeight / 2 - 22, kTerminal - 4, 44, kInk);
    canvas.fillRect(x + 2 * kStroke, y + 2 * kStroke, 22, kHeight - 4 * kStroke, kInk);  // the last sliver
    const char* text = "請充電";
    canvas.drawText((800 - measureText(text, kNotoSansTcBold30)) / 2, y + kHeight + 40, text, kNotoSansTcBold30, kInk);
}

}  // namespace calendar_view
