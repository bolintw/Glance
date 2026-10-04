#pragma once

#include <string>

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Station-mode WiFi: connect and block until connected or timed out. NVS
// must already be initialized (settings::initStorage) -- the WiFi driver
// keeps its calibration data there.
class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    esp_err_t connect(const std::string& ssid, const std::string& password, uint32_t timeoutMs);

private:
    static constexpr int kMaxRetries = 5;
    static constexpr int kConnectedBit = BIT0;
    static constexpr int kFailBit = BIT1;

    static void onWifiOrIpEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    void handleEvent(esp_event_base_t base, int32_t id, void* data);

    EventGroupHandle_t events_;
    int retryCount_ = 0;
    esp_event_handler_instance_t wifiHandler_ = nullptr;
    esp_event_handler_instance_t ipHandler_ = nullptr;
};
