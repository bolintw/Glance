#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "display.hpp"
#include "epd_7in5_v2_regs.hpp"
#include "gpio.hpp"
#include "spi.hpp"

struct FrameSize {
    size_t width;
    size_t height;

    constexpr size_t bytesPerRow() const { return width / 8; }
    constexpr size_t framebufferSize() const { return width * height / 8; }
};

struct EpdConfig {
    FrameSize frame;
    Spi& spiDevice;
    uint8_t dcPin;
    uint8_t resetPin;
    uint8_t busyPin;
};

// Modern-C++ port of Waveshare's 7.5" e-Paper V2 (SSD1683) reference driver.
// Only the full-refresh black/white path is implemented -- fast/partial/4-gray
// modes from the vendor SDK aren't needed yet and can be added when a
// milestone actually calls for them.
class Epd7in5V2 : public Display {
public:
    explicit Epd7in5V2(const EpdConfig& config);

    void init() override;
    void clear() override;
    void flush(std::span<const uint8_t> framebuffer) override;
    void sleep() override;

private:
    // Busy pin polling loses its meaning if the panel is genuinely stuck
    // (bad wiring, dead panel) -- this bounds how long init/refresh/sleep
    // are willing to wait before giving up and logging an error instead of
    // hanging the boot forever.
    static constexpr uint32_t kBusyTimeoutMs = 10000;

    void sendCommand(epd::Command cmd);
    void sendData(uint8_t data);
    void sendData(std::span<const uint8_t> data);
    // Returns false on timeout.
    bool waitUntilIdle(uint32_t timeoutMs = kBusyTimeoutMs);
    void turnOnDisplay();
    void resetHardware();

    void setBoosterSoftStart();
    void setPowerSetting();
    void setPanelSetting();
    void setResolutionSetting();
    void setVcomAndDataInterval();
    void setTconSetting();

    FrameSize frame_;
    Spi& spi_;
    Gpio dc_;
    Gpio reset_;
    Gpio busy_;
};
