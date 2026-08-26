#pragma once

#include <cstdint>
#include <span>

// int host_id / no ESP-IDF types here on purpose: keeps this header
// free of driver includes so it stays easy to reason about (and swap)
// independently of the SPI backend implementation.
struct SpiConfig {
    int mosiPin = -1;
    int misoPin = -1;
    int sclkPin;
    int csPin;
    int hostId;
    uint32_t clockSpeedHz;
    size_t maxTransferSize;
};

// RAII wrapper around one SPI bus + attached device. Bus and device are
// both torn up on construction; there is currently no need for a
// multi-device bus, so this class owns the whole bus.
class Spi {
public:
    explicit Spi(const SpiConfig& config);

    Spi(const Spi&) = delete;
    Spi& operator=(const Spi&) = delete;

    void write(std::span<const uint8_t> data);

private:
    void* deviceHandle_;
};
