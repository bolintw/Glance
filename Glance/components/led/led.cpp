#include "led.hpp"

Led::Led(std::uint8_t pin_num, bool set_active_high) : gpio(Gpio(pin_num, Gpio::Dir::output)), active_high(set_active_high) {
    off();
}

void Led::on(void) {
    gpio.write(active_high);
    is_on = true;
}

void Led::off(void) {
    gpio.write(!active_high);
    is_on = false;
}

void Led::toggle(void) {
    is_on ? off() : on();
}