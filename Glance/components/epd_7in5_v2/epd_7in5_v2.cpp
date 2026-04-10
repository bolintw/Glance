#include "epd_7in5_v2.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "Epd7in5V2";

#define DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))

Epd7in5V2::Epd7in5V2(const struct FrameSize& frame, Spi& spi_device, 
                         std::uint8_t dc_pin, std::uint8_t reset_pin, 
                         std::uint8_t busy_pin, std::uint8_t power_pin)
    : frame_size(frame), spi(spi_device),
      dc(dc_pin, Gpio::Dir::output),
      reset(reset_pin, Gpio::Dir::output),
      busy(busy_pin, Gpio::Dir::input),
      power(power_pin, Gpio::Dir::output) 
{
}

void Epd7in5V2::init(void) {
    power_on();
    reset_hardware();

    set_booster_soft_start();
    set_power_setting();

    ESP_LOGI(TAG, "e-Paper power on sequence");
    send_command(epd::Command::PON);
    DELAY_MS(100);
    wait_until_idle();

    set_panel_setting();
    set_resolution_setting();
    
    // Dual SPI mode disable
    send_command(epd::Command::DUSPI);
    send_data(0x00);

    set_vcom_and_data_interval();
    set_tcon_setting();
}

void Epd7in5V2::init_fast(void) {
    power_on();
    reset_hardware();
    
    set_panel_setting();
    set_vcom_and_data_interval();

    send_command(epd::Command::PON);
    DELAY_MS(100);
    wait_until_idle();

    // Specific booster soft start for fast mode
    send_command(epd::Command::BTST);
    send_data(epd::Config::Booster::FAST_A);
    send_data(epd::Config::Booster::FAST_B);
    send_data(epd::Config::Booster::FAST_C);
    send_data(epd::Config::Booster::PHASE_D);

    send_command(epd::Command::CCSET);
    send_data(0x02);
    send_command(epd::Command::TSSET);
    send_data(0x5A);
}

void Epd7in5V2::init_part(void) {
    power_on();
    reset_hardware();

    set_panel_setting();

    send_command(epd::Command::PON);
    DELAY_MS(100);
    wait_until_idle();

    send_command(epd::Command::CCSET);
    send_data(0x02);
    send_command(epd::Command::TSSET);
    send_data(0x6E);
}

void Epd7in5V2::init_4gray(void) {
    power_on();
    reset_hardware();

    set_panel_setting();
    set_vcom_and_data_interval();

    send_command(epd::Command::PON);
    DELAY_MS(100);
    wait_until_idle();

    // Specific booster soft start for 4gray mode
    send_command(epd::Command::BTST);
    send_data(epd::Config::Booster::FAST_A);
    send_data(epd::Config::Booster::FAST_B);
    send_data(epd::Config::Booster::FAST_C);
    send_data(epd::Config::Booster::PHASE_D);

    send_command(epd::Command::CCSET);
    send_data(0x02);
    send_command(epd::Command::TSSET);
    send_data(0x5F);
}

void Epd7in5V2::set_booster_soft_start(void) {
    ESP_LOGI(TAG, "Setting booster soft start");
    send_command(epd::Command::BTST);
    send_data(epd::Config::Booster::PHASE_A);
    send_data(epd::Config::Booster::PHASE_B);
    send_data(epd::Config::Booster::PHASE_C);
    send_data(epd::Config::Booster::PHASE_D);
}

void Epd7in5V2::set_power_setting(void) {
    ESP_LOGI(TAG, "Setting power parameters");
    send_command(epd::Command::PWR);
    send_data(epd::Config::Power::VGH_20V);
    send_data(epd::Config::Power::VGL_NEG_20V);
    send_data(epd::Config::Power::VDH_15V);
    send_data(epd::Config::Power::VDL_NEG_15V);
}

void Epd7in5V2::set_panel_setting(void) {
    ESP_LOGI(TAG, "Setting panel parameters");
    send_command(epd::Command::PSR);
    send_data(epd::Config::Panel::SCAN_UP | 
              epd::Config::Panel::SHIFT_RIGHT | 
              epd::Config::Panel::BOOSTER_ON | 
              epd::Config::Panel::NO_RESET | 0x01);
}

void Epd7in5V2::set_resolution_setting(void) {
    ESP_LOGI(TAG, "Setting resolution: %dx%d", (int)frame_size.width, (int)frame_size.height);
    send_command(epd::Command::TRES);
    send_data(static_cast<uint8_t>(frame_size.width >> 8) & 0x03);
    send_data(static_cast<uint8_t>(frame_size.width & 0xFF));
    send_data(static_cast<uint8_t>(frame_size.height >> 8) & 0x01);
    send_data(static_cast<uint8_t>(frame_size.height & 0xFF));
}

void Epd7in5V2::set_vcom_and_data_interval(void) {
    ESP_LOGI(TAG, "Setting VCOM and data interval");
    send_command(epd::Command::CDI);
    send_data(epd::Config::Border::BLACK);
    send_data(0x07); // 10 hsync (default)
}

void Epd7in5V2::set_tcon_setting(void) {
    ESP_LOGI(TAG, "Setting TCON");
    send_command(epd::Command::TCON);
    send_data(0x22);
}

void Epd7in5V2::clear(void) {
    size_t width_bytes = frame_size.width / 8;
    uint8_t *black_image = new uint8_t[width_bytes];
    uint8_t *white_image = new uint8_t[width_bytes];
    
    std::memset(black_image, 0xFF, width_bytes);
    std::memset(white_image, 0x00, width_bytes);

    send_command(epd::Command::DTM1);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(black_image, width_bytes));
    }

    send_command(epd::Command::DTM2);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(white_image, width_bytes));
    }

    delete[] black_image;
    delete[] white_image;

    turn_on_display();
}

void Epd7in5V2::clear_black(void) {
    size_t width_bytes = frame_size.width / 8;
    uint8_t *black_image = new uint8_t[width_bytes];
    uint8_t *white_image = new uint8_t[width_bytes];
    
    std::memset(black_image, 0x00, width_bytes);
    std::memset(white_image, 0xFF, width_bytes);

    send_command(epd::Command::DTM1);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(black_image, width_bytes));
    }

    send_command(epd::Command::DTM2);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(white_image, width_bytes));
    }

    delete[] black_image;
    delete[] white_image;

    turn_on_display();
}

void Epd7in5V2::show(std::span<const uint8_t> data) {
    size_t width_bytes = frame_size.width / 8;
    
    send_command(epd::Command::DTM1);
    for (size_t j = 0; j < frame_size.height; j++) {
        send_data(data.subspan(j * width_bytes, width_bytes));
    }

    // Invert data for the second part as in original demo
    uint8_t *inverted_data = new uint8_t[data.size()];
    for (size_t i = 0; i < data.size(); i++) {
        inverted_data[i] = ~data[i];
    }

    send_command(epd::Command::DTM2);
    for (size_t j = 0; j < frame_size.height; j++) {
        send_data(std::span<const uint8_t>(inverted_data + j * width_bytes, width_bytes));
    }

    delete[] inverted_data;

    turn_on_display();
}

void Epd7in5V2::show_part(std::span<const uint8_t> data, uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end) {
    send_command(epd::Command::CDI);
    send_data(0xA9);
    send_data(0x07);

    send_command(epd::Command::PTIN);
    send_command(epd::Command::PTL);

    send_data(static_cast<uint8_t>(x_start >> 8));
    send_data(static_cast<uint8_t>(x_start & 0xFF));
    send_data(static_cast<uint8_t>(x_end >> 8));
    send_data(static_cast<uint8_t>((x_end - 1) & 0xFF));

    send_data(static_cast<uint8_t>(y_start >> 8));
    send_data(static_cast<uint8_t>(y_start & 0xFF));
    send_data(static_cast<uint8_t>(y_end >> 8));
    send_data(static_cast<uint8_t>((y_end - 1) & 0xFF));
    
    send_data(0x01); // Scan mode

    send_command(epd::Command::DTM2);
    spi.write(data);

    turn_on_display();
}

void Epd7in5V2::show_4gray(std::span<const uint8_t> data) {
    size_t pixel_count = frame_size.width * frame_size.height;
    size_t plane_size = pixel_count / 8;
    
    send_command(epd::Command::DTM1);
    for (size_t i = 0; i < plane_size; i++) {
        uint8_t temp3 = 0;
        for (int j = 0; j < 2; j++) {
            uint8_t temp1 = data[i * 2 + j];
            for (int k = 0; k < 2; k++) {
                uint8_t temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0 || temp2 == 0x40) temp3 |= 0x00;
                else temp3 |= 0x01;
                temp3 <<= 1;
                temp1 <<= 2;
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0 || temp2 == 0x40) temp3 |= 0x00;
                else temp3 |= 0x01;
                if (j != 1 || k != 1) temp3 <<= 1;
                temp1 <<= 2;
            }
        }
        send_data(temp3);
    }

    send_command(epd::Command::DTM2);
    for (size_t i = 0; i < plane_size; i++) {
        uint8_t temp3 = 0;
        for (int j = 0; j < 2; j++) {
            uint8_t temp1 = data[i * 2 + j];
            for (int k = 0; k < 2; k++) {
                uint8_t temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0 || temp2 == 0x80) temp3 |= 0x00;
                else temp3 |= 0x01;
                temp3 <<= 1;
                temp1 <<= 2;
                temp2 = temp1 & 0xC0;
                if (temp2 == 0xC0 || temp2 == 0x80) temp3 |= 0x00;
                else temp3 |= 0x01;
                if (j != 1 || k != 1) temp3 <<= 1;
                temp1 <<= 2;
            }
        }
        send_data(temp3);
    }

    turn_on_display();
}

void Epd7in5V2::sleep(void) {
    send_command(epd::Command::CDI);
    send_data(epd::Config::Border::FLOATING);
    send_command(epd::Command::POF);
    wait_until_idle();
    send_command(epd::Command::DSLP);
    send_data(epd::Config::Sleep::KEEP_RAM);
}

void Epd7in5V2::send_command(epd::Command cmd) {
    dc.write(false); // 0 for command
    uint8_t cmd_byte = static_cast<uint8_t>(cmd);
    spi.write(std::span<const uint8_t>(&cmd_byte, 1));
}

void Epd7in5V2::send_data(uint8_t data) {
    dc.write(true); // 1 for data
    spi.write(std::span<const uint8_t>(&data, 1));
}

void Epd7in5V2::send_data(std::span<const uint8_t> data) {
    dc.write(true); // 1 for data
    spi.write(data);
}

void Epd7in5V2::wait_until_idle(void) {
    ESP_LOGI(TAG, "e-Paper busy");
    do {
        send_command(epd::Command::FLG);
        DELAY_MS(5);
    } while (busy.read() == false);
    DELAY_MS(20);
    ESP_LOGI(TAG, "e-Paper busy release");
}

void Epd7in5V2::turn_on_display(void) {
    ESP_LOGI(TAG, "e-Paper turn on display");
    send_command(epd::Command::DRF);
    DELAY_MS(100);
    wait_until_idle();
}

void Epd7in5V2::power_on(void) {
    ESP_LOGI(TAG, "e-Paper power on");
    power.write(true);
    DELAY_MS(100);
}

void Epd7in5V2::reset_hardware(void) {
    ESP_LOGI(TAG, "e-Paper reset");
    reset.write(true);
    DELAY_MS(200);
    reset.write(false);
    DELAY_MS(200);
    reset.write(true);
    DELAY_MS(200);
}
