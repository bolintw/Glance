#pragma once

// The battery, checked first thing in a run (before WiFi loads it).
namespace glance {

// Reads the fuel gauge (and, on V3.0, whether USB is in and charging) into
// this run's wake log entry and sets gLowBattery.
// Too low to run (see run_policy::batteryTooLow): puts up the charge
// reminder once, then sleeps an hour at a time, checking only the gauge,
// until it's charged -- so in that case it doesn't return. Presses are
// ignored meanwhile: a refresh could pull the voltage under brownout.
void checkBattery();

}  // namespace glance
