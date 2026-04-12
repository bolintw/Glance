#include "time_manager.hpp"
#include <ctime>
#include <vector>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TimeManager";

esp_err_t TimeManager::sync() {
    ESP_LOGI(TAG, "Initializing SNTP for ESP-IDF v6.0...");

    // Use the official macro to get a base config, ensuring all internal fields are correct
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");

    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for time synchronization (up to 10 seconds)
    int retry = 0;
    const int max_retry = 10;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000)) != ESP_OK) {
        if (++retry >= max_retry) {
            ESP_LOGE(TAG, "NTP Sync failed after timeout.");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, max_retry);
    }

    ESP_LOGI(TAG, "NTP Sync success!");
    return ESP_OK;
}

void TimeManager::set_timezone(const std::string& tz) {
    setenv("TZ", tz.c_str(), 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to: %s", tz.c_str());
}

bool TimeManager::is_time_set() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 如果年份小於 2000，表示時間還沒設好 (預設通常是 1970)
    return timeinfo.tm_year > (2000 - 1900);
}

std::string TimeManager::get_formatted_time() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buf);
}
