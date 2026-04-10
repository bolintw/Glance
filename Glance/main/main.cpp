#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "epd_7in5_v2.hpp"
#include "spi.hpp"

static const char *TAG = "main";

extern "C" {
    extern uint8_t photo[];
}

// Pin definitions from netlist
#define PIN_NUM_MOSI 35
#define PIN_NUM_CLK  36
#define PIN_NUM_CS   37
#define PIN_NUM_DC   38
#define PIN_NUM_N_RST 39
#define PIN_NUM_BUSY 40
#define PIN_NUM_PWR  14

// Additional pins
#define PIN_NUM_LED  48
#define PIN_NUM_BTN  1
#define PIN_NUM_SDA  3
#define PIN_NUM_SCL  46
#define PIN_NUM_ALRT 9
#define PIN_NUM_FLS_CS 21
#define PIN_NUM_MISO 47
#define PIN_NUM_BOOT 0
#define PIN_NUM_RTC_XTAL_P 15
#define PIN_NUM_RTC_XTAL_N 16
#define PIN_NUM_USB_D_N 19
#define PIN_NUM_USB_D_P 20

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello, please wait 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // SPI Configuration
    SpiConfig spi_config;
    spi_config.mosi_pin = PIN_NUM_MOSI;
    spi_config.miso_pin = -1;
    spi_config.sclk_pin = PIN_NUM_CLK;
    spi_config.cs_pin = PIN_NUM_CS;
    spi_config.host_id = 1; // SPI2_HOST is usually 1
    spi_config.clock_speed_hz = 1 * 1000 * 1000; // 2MHz as in demo
    spi_config.max_transfer_size = 800 * 480 / 8;

    Spi spi_device(spi_config);

    // E-Ink Display Configuration
    FrameSize frame = {
        .width = 800,
        .height = 480,
        .bits_per_pixel = 1
    };

    Epd7in5V2 display(frame, spi_device, PIN_NUM_DC, PIN_NUM_N_RST, PIN_NUM_BUSY, PIN_NUM_PWR);

    ESP_LOGI(TAG, "Initializing display...");
    display.init();
    
    ESP_LOGI(TAG, "Clearing display...");
    display.clear();
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Showing image...");
    display.show(std::span<const uint8_t>(photo, frame.frame_buf_size));
    
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Re-initializing and clearing...");
    display.init();
    display.clear();

    ESP_LOGI(TAG, "Display go to sleep");
    display.sleep();

    while (true) {
        ESP_LOGI(TAG, "Hello, ESP32-S3 ePaper!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
