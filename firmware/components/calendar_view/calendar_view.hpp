#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "canvas.hpp"
#include "ics_event_collector.hpp"
#include "weather.hpp"

// The calendar screen. Pure C++ so the firmware and the host preview
// (firmware/test/host, `make preview`) draw exactly the same thing.
namespace calendar_view {

// Draws the whole screen: today's date header, the weather (left blank
// without a forecast) and up to ten upcoming events in two columns. `now`
// and `utcOffset` decide what "today" is and how event dates are written.
void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events,
            const std::optional<weather::Forecast>& forecast);

// Privacy mode: the same header and weather, with a photo in the event box
// instead of the events. `photo` should be photos::kWidth x photos::kHeight;
// without a clock (`now` nullopt) the date is left out. `offline` puts a
// crossed-out WiFi icon in the top-right corner -- status icons show only
// when something is wrong.
void renderPrivate(Canvas& canvas, std::optional<int64_t> now, int32_t utcOffset,
                   const std::optional<weather::Forecast>& forecast, const Bitmap& photo, bool offline = false);

// The 4-gray version of renderPrivate's photo, placed over the same frame,
// for Display::flushGray. Draw the 1bpp photo underneath anyway: backends
// without a 4-gray mode show only that.
GrayOverlay photoOverlay(const GrayBitmap& photo);

}  // namespace calendar_view
