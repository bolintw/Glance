#pragma once

#include <cstdint>

// RAII wrapper for a single GPIO pin: resets and configures the direction
// and pull on construction so a pin's mode is never left implicit.
class Gpio {
public:
    enum class Direction { input, output };
    enum class Pull { none, up, down };

    Gpio(uint8_t pinNumber, Direction direction, Pull pull = Pull::none);

    Gpio(const Gpio&) = delete;
    Gpio& operator=(const Gpio&) = delete;

    void write(bool level);
    bool read() const;

private:
    uint8_t pinNumber_;
    Direction direction_;
};
