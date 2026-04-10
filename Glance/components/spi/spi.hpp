#pragma once

#include <cstdint>
#include <span>

struct SpiConfig {
    int mosi_pin = -1;
    int miso_pin = -1;
    int sclk_pin;
    int cs_pin;
    int host_id;
    uint32_t clock_speed_hz;
    size_t max_transfer_size;
};

class Spi {
public:
    Spi(const SpiConfig& config);
    bool write(std::span<const uint8_t> data);

    Spi(const Spi&) = delete;
    Spi& operator=(const Spi&) = delete;
private:
    void *handle_t;
};