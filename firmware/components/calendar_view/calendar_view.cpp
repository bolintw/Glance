#include "calendar_view.hpp"

#include <cstdio>
#include <string>

#include "fonts.hpp"
#include "icons.hpp"
#include "photos.hpp"
#include "ics_datetime.hpp"

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

constexpr const char* kMonths[] = {"Jan.", "Feb.", "Mar.", "Apr.", "May.", "Jun.",
                                   "Jul.", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
constexpr const char* kWeekdays[] = {"一", "二", "三", "四", "五", "六", "日"};  // Monday first

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

ics::Date localDate(int64_t unix, int32_t utcOffset) { return ics::civilFromDays(floorDiv(unix + utcOffset, 86400)); }

// The Pi version's row separators were grey; on a 1bpp panel a dotted line
// reads as the lighter rule.
void dottedRule(Canvas& canvas, int x, int y, int width) {
    for (int i = 0; i < width; i += 2) {
        canvas.setPixel(x + i, y, kInk);
    }
}

void drawOnBaseline(Canvas& canvas, int x, int baseline, std::string_view text, const Font& font) {
    canvas.drawText(x, baseline - font.ascent, text, font, kInk);
}

void drawHeader(Canvas& canvas, ics::Date today) {
    char text[8];

    std::snprintf(text, sizeof(text), "%d", today.year);
    drawOnBaseline(canvas, kYearX, kYearBaseline, text, kNotoSansTcBold40);

    const char* month = kMonths[today.month - 1];
    drawOnBaseline(canvas, kMonthRight - measureText(month, kNotoSansTcMedium50), kHeaderBaseline, month,
                   kNotoSansTcMedium50);

    std::snprintf(text, sizeof(text), "%d", today.day);
    drawOnBaseline(canvas, kDayCenter - measureText(text, kNotoSansTcRegular100) / 2, kHeaderBaseline, text,
                   kNotoSansTcRegular100);

    drawOnBaseline(canvas, kWeekdayX, kWeekdayBaseline, kWeekdays[ics::weekdayOf(today)], kNotoSansTcBold40);

    for (int x : kHeaderDividerX) {
        canvas.fillRect(x, kHeaderDividerTop, 1, kHeaderDividerBottom - kHeaderDividerTop, kInk);
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

void drawWeather(Canvas& canvas, const weather::Forecast& forecast) {
    drawOnBaseline(canvas, kWeatherIconX, kWeatherIconBaseline, iconFor(forecast.condition), kWeatherIcons);

    char text[24];
    std::snprintf(text, sizeof(text), "%d° - %d°", forecast.minTemp, forecast.maxTemp);
    drawOnBaseline(canvas, kTemperatureCenter - measureText(text, kNotoSansTcBold20) / 2, kWeatherTextBaseline, text,
                   kNotoSansTcBold20);

    drawOnBaseline(canvas, kRaindropX, kRaindropBaseline, icons::kRaindrop, kWeatherIcons);
    std::snprintf(text, sizeof(text), "%d%%", forecast.rainChance);
    drawOnBaseline(canvas, kRainChanceX, kWeatherTextBaseline, text, kNotoSansTcBold20);
}

void drawBox(Canvas& canvas) { canvas.roundedRect(kBoxX, kBoxY, kBoxWidth, kBoxHeight, kBoxRadius, 2, kInk); }

void drawEventBox(Canvas& canvas, int32_t utcOffset, std::span<const ics::Occurrence> events) {
    drawBox(canvas);
    canvas.fillRect(kColumnDividerX, kBoxY, 2, kBoxHeight, kInk);
    for (int column = 0; column < 2; column++) {
        for (int row = 1; row <= kRowsPerColumn; row++) {  // a rule under every row
            dottedRule(canvas, kColumnRuleX[column], kFirstRowY + row * kRowPitch - 5, kRuleWidth);
        }
    }

    if (events.empty()) {
        canvas.drawText(kColumnTextX[0], kFirstRowY, "沒有接下來的行程", kNotoSansTcBold30, kInk);
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
                        ellipsize(line, kNotoSansTcBold30, kMaxTextWidth), kNotoSansTcBold30, kInk);
    }
}

}  // namespace

void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
            const std::optional<weather::Forecast>& forecast) {
    canvas.fill(kBackground);
    drawHeader(canvas, localDate(now, utcOffset));
    if (forecast) {
        drawWeather(canvas, *forecast);
    }
    drawEventBox(canvas, utcOffset, events);
}

void renderPrivate(Canvas& canvas, std::optional<int64_t> now, int32_t utcOffset,
                   const std::optional<weather::Forecast>& forecast, const Bitmap& photo) {
    canvas.fill(kBackground);
    if (now) {
        drawHeader(canvas, localDate(*now, utcOffset));
    }
    if (forecast) {
        drawWeather(canvas, *forecast);
    }
    drawBox(canvas);
    canvas.drawBitmap(kPhotoX, kPhotoY, photo);
}

}  // namespace calendar_view
