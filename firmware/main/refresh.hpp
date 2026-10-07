#pragma once

#include "device_settings.hpp"

// One run's refresh, either way the screen can look. Both fill in this
// run's wake log result. `settings` follows if the WiFi had to fall back to
// the last network that worked.
namespace glance {

// WiFi -> NTP -> calendars + weather -> render -> panel. If any step before
// rendering fails, the panel is left alone: e-ink keeps showing the last
// good calendar, which beats replacing it with a wrong date or an empty
// list -- unless the offline screen is due (run_policy::offlineScreenDue).
// Weather is the exception -- without it the calendar is still worth
// showing, just with the weather spot left blank. True once the new
// calendar is on the panel.
bool refresh(Settings& settings);

// Privacy mode (and the photo frame): date, weather and a photo, no
// calendars fetched at all. Unlike the calendar it always redraws, even
// without WiFi or a synced clock -- the point is that the events come off
// the screen. True if it got everything; false means retry later for the
// date and weather.
bool refreshPrivate(Settings& settings);

}  // namespace glance
