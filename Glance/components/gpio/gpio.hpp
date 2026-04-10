#pragma once

#include <cstdint>

class Gpio {
public:
    enum class Dir {
        input,
        output
    };
    Gpio(std::uint8_t pin_num, Gpio::Dir pin_dir);
    void write(bool gpio_level);
    bool read();

    Gpio(const Gpio&) = delete;
    Gpio& operator=(const Gpio&) = delete;
private:
    std::uint8_t pin_num;
    Gpio::Dir direction;
};