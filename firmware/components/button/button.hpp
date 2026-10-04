#pragma once

#include <cstdint>

#include "gpio.hpp"

// A push button wired active-low with an external pull-up (on this board,
// IO1 with R31 to the always-on 3V3, so it reads correctly even with the
// peripheral rail off).
class Button {
public:
    enum class Press { none, shortPress, longPress };

    explicit Button(uint8_t pin);

    bool isDown() const;

    // If the button is down (debounced): waits for it to be released
    // (shortPress) or to be held for longPressMs (longPress, returned while
    // it's still held, so the device reacts without waiting for release).
    // none if it isn't down.
    Press readPress(uint32_t longPressMs) const;

private:
    Gpio pin_;
};
