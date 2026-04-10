#pragma once

#include "gpio.hpp"

class Led {
public:
    Led(std::uint8_t pin_num, bool set_active_high = true);
    void on(void);
    void off(void);
    void toggle(void);
private:
    Gpio gpio;
    bool active_high = true;
    bool is_on = false;
};