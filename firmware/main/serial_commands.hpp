#pragma once

// Development aid (GLANCE_DEV_SERIAL_COMMANDS): lines typed on the USB serial
// console that stand in for the button, so tests can drive it without anyone
// there.
//   glance press short | glance press long
//   glance privacy on | glance privacy off
//   glance refresh
//   glance refresh configured    (as if setup had just saved: WiFi fallback, offline screen at once)
//   glance wifi <network name>   (keeps the saved password)
//   glance wifi default          (back to the menuconfig network and password)
//   glance log                   (the wake log, as CSV)
//   glance sleep                 (deep sleep until the button: for measuring sleep current)
//   glance battery <0-100>       (the next boot sees this state of charge: tries the low-battery paths)
namespace glance {

// A FreeRTOS task; only started in development builds.
void serialCommandTask(void*);

}  // namespace glance
