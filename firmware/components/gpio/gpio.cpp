#include "gpio.hpp"

#include "driver/gpio.h"

Gpio::Gpio(uint8_t pinNumber, Direction direction) : pinNumber_(pinNumber), direction_(direction) {
    auto pin = static_cast<gpio_num_t>(pinNumber_);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, direction_ == Direction::input ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT);
}

void Gpio::write(bool level) {
    if (direction_ == Direction::input) {
        return;
    }
    gpio_set_level(static_cast<gpio_num_t>(pinNumber_), level);
}

bool Gpio::read() const {
    return gpio_get_level(static_cast<gpio_num_t>(pinNumber_));
}
