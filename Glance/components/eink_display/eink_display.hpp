#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "spi.hpp"
#include "gpio.hpp"

struct FrameSize{
    size_t width;
    size_t height;
    size_t bits_per_pixel;
    const size_t frame_buf_size = (width * height * bits_per_pixel) / 8;
};

class EinkDisplay {
public:
    EinkDisplay(const struct FrameSize& frame, Spi& spi_device, 
                std::uint8_t dc_pin, std::uint8_t reset_pin, 
                std::uint8_t busy_pin, std::uint8_t power_pin);
    
    void init(void);
    void clear(void);
    void show(std::span<const uint8_t> data);
    void sleep(void);

private:
    void send_command(uint8_t cmd);
    void send_data(uint8_t data);
    void send_data(std::span<const uint8_t> data);
    void wait_until_idle(void);
    void turn_on_display(void);
    void power_on(void);
    void reset_hardware(void);

    struct FrameSize frame_size;
    Spi& spi;
    Gpio dc;
    Gpio reset;
    Gpio busy;
    Gpio power;
};
