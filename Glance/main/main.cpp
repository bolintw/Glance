#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "epd_7in5_v2.hpp"
#include "spi.hpp"
#include "glance_board.hpp"
#include "wifi.hpp"
#include "time_manager.hpp"
#include "config.hpp"
#include "storage.hpp"

static const char *TAG = "main";

extern "C" {
    extern uint8_t photo[];
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hello, please wait 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Mount storage and load config
    AppConfig app_cfg;
    if (Storage::init("/spiffs", "storage") == ESP_OK) {
        Storage::list_files();
        ConfigManager cfg_mgr("/spiffs/config.json");
        if (cfg_mgr.load() == ESP_OK) {
            app_cfg = cfg_mgr.get();
            ESP_LOGI(TAG, "Config loaded for WiFi: %s", app_cfg.wifi.ssid.c_str());
        }
    }

    // Connect to WiFi
    Wifi& wifi = Wifi::get_instance();
    if (!app_cfg.wifi.ssid.empty()) {
        if (wifi.connect(app_cfg.wifi.ssid, app_cfg.wifi.password) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected successfully!");
            
            // Sync time via NTP
            TimeManager& time_mgr = TimeManager::get_instance();
            if (time_mgr.sync() == ESP_OK) {
                // Set timezone to Taipei (UTC+8)
                time_mgr.set_timezone("CST-8");
                ESP_LOGI(TAG, "Current time: %s", time_mgr.get_formatted_time().c_str());
            }
        } else {
            ESP_LOGE(TAG, "WiFi connection failed.");
        }
    } else {
        ESP_LOGE(TAG, "WiFi SSID is empty in config!");
    }

    // SPI Configuration
    SpiConfig spi_config;
    spi_config.mosi_pin = board::MOSI;
    spi_config.miso_pin = -1; // MISO is not needed for EPD writes
    spi_config.sclk_pin = board::CLK;
    spi_config.cs_pin = board::EPD_CS;
    spi_config.host_id = 1; // SPI2_HOST
    spi_config.clock_speed_hz = 1 * 1000 * 1000;
    spi_config.max_transfer_size = 800 * 480 / 8;

    Spi spi_device(spi_config);

    // Power on the V33_2 rail (Controls EPD, Flash, and LED)
    Gpio power(board::PWR, Gpio::Dir::output);
    power.write(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    // E-Ink Display Configuration
    EpdConfig epd_config = {
        .frame = {
            .width = 800,
            .height = 480,
            .bits_per_pixel = 1
        },
        .spi_device = spi_device,
        .dc_pin = board::EPD_DC,
        .reset_pin = board::EPD_RST,
        .busy_pin = board::EPD_BUSY,
    };

    Epd7in5V2 display(epd_config);

    ESP_LOGI(TAG, "Initializing display...");
    display.init();
    
    ESP_LOGI(TAG, "Clearing display...");
    display.clear();
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Showing image...");
    display.show(std::span<const uint8_t>(photo, epd_config.frame.frame_buf_size));
    
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Re-initializing and clearing...");
    display.init();
    display.clear();

    ESP_LOGI(TAG, "Display go to sleep");
    display.sleep();

    TimeManager& time_mgr = TimeManager::get_instance();
    while (true) {
        ESP_LOGI(TAG, "Current Time: %s", time_mgr.get_formatted_time().c_str());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
