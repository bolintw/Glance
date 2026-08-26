#include "wifi_manager.hpp"

#include <cstdio>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace {
constexpr const char* kTag = "wifi_manager";
}

WifiManager::WifiManager() {
    events_ = xEventGroupCreate();

    esp_err_t nvsResult = nvs_flash_init();
    if (nvsResult == ESP_ERR_NVS_NO_FREE_PAGES || nvsResult == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsResult = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsResult);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::onWifiOrIpEvent, this, &wifiHandler_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::onWifiOrIpEvent, this, &ipHandler_));
}

WifiManager::~WifiManager() {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiHandler_);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ipHandler_);
    vEventGroupDelete(events_);
}

void WifiManager::onWifiOrIpEvent(void* arg, esp_event_base_t base, int32_t id, void* data) {
    static_cast<WifiManager*>(arg)->handleEvent(base, id, data);
}

void WifiManager::handleEvent(esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retryCount_ < kMaxRetries) {
            esp_wifi_connect();
            retryCount_++;
            ESP_LOGW(kTag, "disconnected, retry %d/%d", retryCount_, kMaxRetries);
        } else {
            xEventGroupSetBits(events_, kFailBit);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(kTag, "got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retryCount_ = 0;
        xEventGroupSetBits(events_, kConnectedBit);
    }
}

esp_err_t WifiManager::connect(uint32_t timeoutMs) {
    wifi_config_t wifiConfig = {};
    std::snprintf(reinterpret_cast<char*>(wifiConfig.sta.ssid), sizeof(wifiConfig.sta.ssid), "%s",
                  CONFIG_GLANCE_WIFI_SSID);
    std::snprintf(reinterpret_cast<char*>(wifiConfig.sta.password), sizeof(wifiConfig.sta.password),
                  "%s", CONFIG_GLANCE_WIFI_PASSWORD);
    wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(events_, kConnectedBit | kFailBit, pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(timeoutMs));
    if (bits & kConnectedBit) {
        return ESP_OK;
    }
    if (bits & kFailBit) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}
