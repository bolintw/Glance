#include "config.hpp"
#include "json_reader.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include "esp_log.h"

static const char *TAG = "Config";

ConfigManager::ConfigManager(const std::string& path) : file_path(path) {}

esp_err_t ConfigManager::load() {
    FILE* f = fopen(file_path.c_str(), "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open config file for reading");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_str_ptr = (char *)malloc(fsize + 1);
    fread(json_str_ptr, fsize, 1, f);
    fclose(f);
    json_str_ptr[fsize] = 0;
    std::string json_str(json_str_ptr);
    free(json_str_ptr);

    JsonReader root;
    if (!root.parse(json_str)) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    JsonReader wifi = root.get_object("wifi");
    config.wifi.ssid = wifi.get_string("ssid");
    config.wifi.password = wifi.get_string("password");

    JsonReader calendar = root.get_object("calendar");
    config.calendar.urls = calendar.get_string_array("urls");

    ESP_LOGI(TAG, "Config loaded successfully");
    return ESP_OK;
}
