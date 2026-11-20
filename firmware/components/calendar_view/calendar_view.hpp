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

// The calendar's colors: white on black, or black on white. (Privacy mode
// is the photo with a white label, whatever the theme.)
enum class Theme : uint8_t { dark, light };

// Draws the whole screen: today's date header, the weather (left blank
// without a forecast) and up to ten upcoming events in two columns. `now`
// and `utcOffset` decide what "today" is and how event dates and times are
// written. `showTimes` adds each event's start and end (ALL DAY for an
// all-day one) between its date and its title.
void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
            const std::optional<weather::Forecast>& forecast, Status status = {}, Theme theme = Theme::dark,
            bool showTimes = false);

// The calendar on a portrait screen (480x800): the date and the weather on
// top, up to twelve upcoming events in one column.
void renderPortrait(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
                    const std::optional<weather::Forecast>& forecast, Status status = {}, Theme theme = Theme::dark,
                    bool showTimes = false);

// Privacy mode: `photo` (the canvas's size, landscape or portrait) fills
// the screen, with a white label in the bottom-right corner -- the date,
// and under it the weather or `cornerQr` (a QR payload: a WiFi to join, a
// web address, any text -- see setup_page::cornerQrPayload). Without a
// clock (`now` nullopt) the date is left out. The photo is drawn in black
// and white here: photoOverlay has it in four grays.
void renderPrivate(Canvas& canvas, std::optional<int64_t> now, int32_t utcOffset,
                   const std::optional<weather::Forecast>& forecast, const GrayBitmap& photo, Status status = {},
                   std::string_view cornerQr = {});

// renderPrivate's photo in four grays, around its label, for
// Display::flushGray: pass what renderPrivate got, which sizes the label. A
// portrait photo makes a portrait overlay (GrayOverlay::portrait).
GrayOverlay photoOverlay(const GrayBitmap& photo, const std::optional<weather::Forecast>& forecast,
                         Status status = {}, std::string_view cornerQr = {});

// The battery is too low to keep going: a big empty battery and "請充電",
// left on the panel while the device sleeps until it's charged. Always dark.
void renderChargeReminder(Canvas& canvas);

}  // namespace calendar_view
