#include "esp_log.h"
#include "wifi_manager.hpp"

namespace {
constexpr const char* kTag = "main";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
}

extern "C" void app_main(void)
{
    WifiManager wifi;
    esp_err_t result = wifi.connect(kWifiConnectTimeoutMs);
    if (result == ESP_OK) {
        ESP_LOGI(kTag, "M1 WiFi regression: connected");
    } else {
        ESP_LOGE(kTag, "M1 WiFi regression: failed (%s)", esp_err_to_name(result));
    }
}
