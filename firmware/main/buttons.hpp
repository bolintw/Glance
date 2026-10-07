#pragma once

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
//   in setup mode, any press leaves it
// `waitForRelease` runs once a long press has lit the LED: setup mode starts
// when the button is let go.
void actOnPress(bool isLong, const std::function<void()>& waitForRelease);

// Watches the button from boot on, so a press is timed from when it really
// started even while a refresh keeps app_main busy. A FreeRTOS task.
void buttonTask(void*);

// A press woke the device from deep sleep. Times it from the wake-up (the
// button task only starts afterwards): held to the long-press time, setup
// mode; otherwise a short press -- privacy mode toggled for this run, or,
// with no calendar to switch to (offline, photo frame), straight back to
// sleep until the timer was due.
void actOnWakePress(bool noCalendars);

}  // namespace glance
