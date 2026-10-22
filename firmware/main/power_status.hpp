#pragma once

#include "wake_log.hpp"

// Whether USB is plugged in and the battery charging (USB_DETEC and
// CHG_STAT, see board.hpp).
namespace glance::power_status {

wake_log::Power read();

// The raw readings, for the development hardware check (`glance hw`).
struct Details {
    int usbMillivolts = -1;     // VUSB, from the divider (-1: no reading)
    int chargeMillivolts = -1;  // at the CHG_STAT pin (-1: no reading)
    bool chargeLineHigh = false;
    wake_log::Power power = wake_log::Power::unknown;
};
Details readDetails();

}  // namespace glance::power_status
