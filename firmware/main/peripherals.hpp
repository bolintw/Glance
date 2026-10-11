#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "display.hpp"

// The peripheral rail (V33_2) and what's on it: the panel and the status LED.
namespace glance {

// Brings up the panel (or its simulator stand-in), shows one frame and puts
// it back to sleep, the rail cut again afterwards. False if the panel didn't
// respond.
bool show(std::span<const uint8_t> framebuffer, const GrayOverlay* overlay = nullptr);

// Stops show() from drawing from now on, first waiting up to waitMs for a
// refresh in progress to finish: cutting the rail mid-waveform leaves the
// panel half drawn (faint). For a run that's giving up. False if the
// refresh was still going when the wait ran out.
bool closePanel(uint32_t waitMs);

// A title and a few centred lines (setup mode's messages).
void showNotice(std::string_view title, std::span<const std::string_view> lines);

// The status LED on (the rail too, since the LED is on it) and off again,
// the rail left on (a refresh may be about to use it).
void statusLedOn();
void statusLedOff();

// End of a run: the rail and the LED off, whether or not the panel was drawn
// (show() cuts the rail itself, but a run that never got to draw left the
// LED lit by the press that started it).
void peripheralsOff();

}  // namespace glance
