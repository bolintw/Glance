#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "canvas.hpp"
#include "ics_event_collector.hpp"
#include "weather.hpp"

// The calendar screen. Pure C++ so the firmware and the host preview
// (firmware/test/host, `make preview`) draw exactly the same thing.
namespace calendar_view {

// Status icons in the top-right corner. They show only when something is
// wrong: nothing at all means all is well.
struct Status {
    bool offline = false;     // couldn't get online (crossed-out WiFi)
    bool lowBattery = false;  // time to charge soon (battery with one bar)
    bool calendarError = false;  // a calendar hasn't come through for two days (calendar with a cross)
    bool weatherError = false;   // online with weather set up, but no forecast came (crossed-out cloud)
};

// The screen's colors: white on black, or black on white. Photos are drawn
// as they are either way.
enum class Theme : uint8_t { dark, light };

// Whether a photo is mostly dark (fewer than half its pixels white): the
// light theme sets it off better.
bool isDarkPhoto(const Bitmap& photo);

// Draws the whole screen: today's date header, the weather (left blank
// without a forecast) and up to ten upcoming events in two columns. `now`
// and `utcOffset` decide what "today" is and how event dates are written.
void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
            const std::optional<weather::Forecast>& forecast, Status status = {}, Theme theme = Theme::dark);

// Privacy mode: the same header and weather, with a photo in the event box
// instead of the events. `photo` should be photos::kWidth x photos::kHeight;
// without a clock (`now` nullopt) the date is left out. With `cornerQr` (a
// QR payload: a WiFi to join, a web address, any text -- see
// setup_page::cornerQrPayload) a QR code takes the weather's place.
void renderPrivate(Canvas& canvas, std::optional<int64_t> now, int32_t utcOffset,
                   const std::optional<weather::Forecast>& forecast, const Bitmap& photo, Status status = {},
                   std::string_view cornerQr = {}, Theme theme = Theme::dark);

// The battery is too low to keep going: a big empty battery and "請充電",
// left on the panel while the device sleeps until it's charged. Always dark.
void renderChargeReminder(Canvas& canvas);

// The 4-gray version of renderPrivate's photo, placed over the same frame,
// for Display::flushGray. Draw the 1bpp photo underneath anyway: backends
// without a 4-gray mode show only that.
GrayOverlay photoOverlay(const GrayBitmap& photo);

}  // namespace calendar_view
