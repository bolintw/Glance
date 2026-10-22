#pragma once

#include <cstdint>
#include <functional>

// The button: what a press means, depending on what the device is doing.
namespace glance {

// What a press does (from the button, or a serial command in development):
//   short: toggle privacy mode and redraw at once (cutting short any
//          refresh in progress: a guest at the door shouldn't wait for it)
//          -- except while the panel shows the offline icon, or with no
//          calendars at all (a photo frame): there's no calendar to switch
//          to, so it's ignored (no blink either: asleep, as the device
//          mostly will be, it couldn't blink anyway)
//   long:  setup mode
//   in setup mode, a long press leaves it
//   held to kEraseHoldMs (the LED blinks fast): erase everything and ship
//          (see eraseAndShip) -- the last resort when the setup page can't
//          be reached, e.g. a WiFi the phone can't get to
// `waitForRelease` runs once a long press has lit the LED: what it asked for
// happens when the button is let go. It says how long it was held -- stuck
// if it never came up (see kStuckButtonMs): then the press is ignored.
enum class Hold { released, heldToErase, stuck };
void actOnPress(bool isLong, const std::function<Hold()>& waitForRelease);

// Held this long (from when it went down) and then let go, a press erases
// everything.
inline constexpr uint32_t kEraseHoldMs = 15 * 1000;

// A button held this long isn't being pressed: it's stuck (a shorted cable,
// something resting on it), and waiting for it to come up would keep the
// device awake.
inline constexpr uint32_t kStuckButtonMs = 30 * 1000;

// Watches the button from boot on, so a press is timed from when it really
// started even while a refresh keeps app_main busy. A FreeRTOS task.
void buttonTask(void*);

// A press woke the device from deep sleep. Times it from the wake-up (the
// button task only starts afterwards): held to the long-press time, setup
// mode (or held to kEraseHoldMs, erase everything); otherwise a short press -- privacy mode toggled for this run, or,
// with no calendar to switch to (offline, photo frame), straight back to
// sleep until the timer was due.
void actOnWakePress(bool noCalendars);

}  // namespace glance
