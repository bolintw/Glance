#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "display.hpp"
#include "epd_7in5_v2_regs.hpp"
#include "gpio.hpp"
#include "spi.hpp"

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

    bool init() override;
    bool clear() override;
    bool flush(std::span<const uint8_t> framebuffer) override;
    // 4-gray mode. Only panels sold after 2023-10 have its waveform (see
    // Waveshare's EPD_7IN5_V2_Init_4Gray); older ones show it wrong. Clears
    // to white first, so it takes two refreshes. Needs init() first.
    bool flushGray(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay) override;
    void sleep() override;

private:
    // Busy pin polling loses its meaning if the panel is genuinely stuck
    // (bad wiring, dead panel) -- this bounds how long init/refresh/sleep
    // are willing to wait before giving up and logging an error instead of
    // hanging the boot forever.
    static constexpr uint32_t kBusyTimeoutMs = 10000;
    // BUSY is pulled down, so with no panel attached it reads "busy"
    // forever. A present panel drives it high within ~200ms of a hardware
    // reset (measured), so not seeing that means there's no panel.
    static constexpr uint32_t kPresenceTimeoutMs = 500;
    // A real full refresh holds BUSY low for seconds. If it never goes low
    // after DRF, the panel ignored the command.
    static constexpr uint32_t kRefreshStartTimeoutMs = 200;

    void sendCommand(epd::Command cmd);
    void sendData(uint8_t data);
    void sendData(std::span<const uint8_t> data);
    // Both return false on timeout.
    bool waitUntilIdle(uint32_t timeoutMs = kBusyTimeoutMs);
    bool waitUntilBusy(uint32_t timeoutMs);
    bool turnOnDisplay();
    void resetHardware();
    void initGrayMode();
    // Sends one of the two RAM planes for 4-gray mode; `second` picks DTM2.
    void sendGrayPlane(std::span<const uint8_t> framebuffer, const GrayOverlay& overlay, bool second);

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
