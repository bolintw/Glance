#include "driver/gpio.h"
#include "gpio.hpp"

Gpio::Gpio(std::uint8_t pin_num, Gpio::Dir pin_dir) : pin_num(pin_num), direction(pin_dir) {
    gpio_num_t gpio_num = static_cast<gpio_num_t>(pin_num);
    gpio_mode_t gpio_mode = (pin_dir == Gpio::Dir::input) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
    gpio_reset_pin(gpio_num);
    gpio_set_direction(gpio_num, gpio_mode);
}

void Gpio::write(bool gpio_level) {
    if(direction == Gpio::Dir::input) {
        return;
    }

    gpio_num_t gpio_num = static_cast<gpio_num_t>(pin_num);
    gpio_set_level(gpio_num, gpio_level);
}

bool Gpio::read() {
    gpio_num_t gpio_num = static_cast<gpio_num_t>(pin_num);
    return gpio_get_level(gpio_num);
}