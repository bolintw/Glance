#pragma once

#include <cstdint>
#include <span>

#include "canvas.hpp"
#include "ics_event_collector.hpp"

// The calendar screen. Pure C++ so the firmware and the host preview
// (firmware/test/host, `make preview`) draw exactly the same thing.
namespace calendar_view {

// Draws the whole screen: today's date header and up to ten upcoming events
// in two columns. `now` and `utcOffset` decide what "today" is and how event
// dates are written.
void render(Canvas& canvas, int64_t now, int32_t utcOffset, std::span<const ics::Occurrence> events);

}  // namespace calendar_view
