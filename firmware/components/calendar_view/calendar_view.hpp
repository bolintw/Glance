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

}  // namespace calendar_view
