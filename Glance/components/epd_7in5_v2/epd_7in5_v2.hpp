#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "spi.hpp"
#include "gpio.hpp"
#include "epd_7in5_v2_regs.hpp"

struct FrameSize{
    size_t width;
    size_t height;
    size_t bits_per_pixel;
    const size_t frame_buf_size = (width * height * bits_per_pixel) / 8;
};

struct EpdConfig {
    FrameSize frame;
    Spi& spi_device;
    std::uint8_t dc_pin;
    std::uint8_t reset_pin;
    std::uint8_t busy_pin;
};

class Epd7in5V2 {
public:
    Epd7in5V2(const EpdConfig& config);
    
    // Initialization variants
    void init(void);
    void init_fast(void);
    void init_part(void);
    void init_4gray(void);

    void clear(void);
    void clear_black(void);
    
    // Display variants
    void show(std::span<const uint8_t> data);
    void show_part(std::span<const uint8_t> data, uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end);
    void show_4gray(std::span<const uint8_t> data);

    void sleep(void);

private:
    void send_command(epd::Command cmd);
    void send_data(uint8_t data);
    void send_data(std::span<const uint8_t> data);
    void wait_until_idle(void);
    void turn_on_display(void);
    void reset_hardware(void);

    // Initialization sub-functions
    void set_booster_soft_start(void);
    void set_power_setting(void);
    void set_panel_setting(void);
    void set_resolution_setting(void);
    void set_vcom_and_data_interval(void);
    void set_tcon_setting(void);

    struct FrameSize frame_size;
    Spi& spi;
    Gpio dc;
    Gpio reset;
    Gpio busy;
};
