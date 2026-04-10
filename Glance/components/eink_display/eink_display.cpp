#include "eink_display.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "EinkDisplay";

#define DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))

EinkDisplay::EinkDisplay(const struct FrameSize& frame, Spi& spi_device, 
                         std::uint8_t dc_pin, std::uint8_t reset_pin, 
                         std::uint8_t busy_pin, std::uint8_t power_pin)
    : frame_size(frame), spi(spi_device),
      dc(dc_pin, Gpio::Dir::output),
      reset(reset_pin, Gpio::Dir::output),
      busy(busy_pin, Gpio::Dir::input),
      power(power_pin, Gpio::Dir::output) 
{
}

void EinkDisplay::init(void) {
    power_on();
    reset_hardware();

    // Initialization sequence from epd_7in5_v2.c
    send_command(0x06);
    send_data(0x17);
    send_data(0x17);
    send_data(0x28);
    send_data(0x17);

    send_command(0x01);
    send_data(0x07);
    send_data(0x07);
    send_data(0x3F);
    send_data(0x3F);

    send_command(0x04);
    DELAY_MS(100);
    wait_until_idle();

    send_command(0x00);
    send_data(0x1F);

    send_command(0x61);
    send_data(0x03);
    send_data(0x20);
    send_data(0x01);
    send_data(0xE0);

    send_command(0x15);
    send_data(0x00);

    send_command(0x50);
    send_data(0x10);
    send_data(0x07);

    send_command(0x60);
    send_data(0x22);
}

void EinkDisplay::clear(void) {
    size_t width_bytes = frame_size.width / 8;
    uint8_t *black_image = new uint8_t[width_bytes];
    uint8_t *white_image = new uint8_t[width_bytes];
    
    std::memset(black_image, 0xFF, width_bytes);
    std::memset(white_image, 0x00, width_bytes);

    send_command(0x10);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(black_image, width_bytes));
    }

    send_command(0x13);
    for(size_t i = 0; i < frame_size.height; i++) {
        send_data(std::span<const uint8_t>(white_image, width_bytes));
    }

    delete[] black_image;
    delete[] white_image;

    turn_on_display();
}

void EinkDisplay::show(std::span<const uint8_t> data) {
    size_t width_bytes = frame_size.width / 8;
    
    send_command(0x10);
    for (size_t j = 0; j < frame_size.height; j++) {
        send_data(data.subspan(j * width_bytes, width_bytes));
    }

    // Invert data for the second part as in original demo
    uint8_t *inverted_data = new uint8_t[data.size()];
    for (size_t i = 0; i < data.size(); i++) {
        inverted_data[i] = ~data[i];
    }

    send_command(0x13);
    for (size_t j = 0; j < frame_size.height; j++) {
        send_data(std::span<const uint8_t>(inverted_data + j * width_bytes, width_bytes));
    }

    delete[] inverted_data;

    turn_on_display();
}

void EinkDisplay::sleep(void) {
    send_command(0x50);
    send_data(0xF7);
    send_command(0x02);
    wait_until_idle();
    send_command(0x07);
    send_data(0xA5);
}

void EinkDisplay::send_command(uint8_t cmd) {
    dc.write(false); // 0 for command
    spi.write(std::span<const uint8_t>(&cmd, 1));
}

void EinkDisplay::send_data(uint8_t data) {
    dc.write(true); // 1 for data
    spi.write(std::span<const uint8_t>(&data, 1));
}

void EinkDisplay::send_data(std::span<const uint8_t> data) {
    dc.write(true); // 1 for data
    spi.write(data);
}

void EinkDisplay::wait_until_idle(void) {
    ESP_LOGI(TAG, "e-Paper busy");
    do {
        send_command(0x71);
        DELAY_MS(5);
    } while (busy.read() == false);
    DELAY_MS(20);
    ESP_LOGI(TAG, "e-Paper busy release");
}

void EinkDisplay::turn_on_display(void) {
    ESP_LOGI(TAG, "e-Paper turn on display");
    send_command(0x12);
    DELAY_MS(100);
    wait_until_idle();
}

void EinkDisplay::power_on(void) {
    ESP_LOGI(TAG, "e-Paper power on");
    power.write(true);
    DELAY_MS(100);
}

void EinkDisplay::reset_hardware(void) {
    ESP_LOGI(TAG, "e-Paper reset");
    reset.write(true);
    DELAY_MS(200);
    reset.write(false);
    DELAY_MS(200);
    reset.write(true);
    DELAY_MS(200);
}
