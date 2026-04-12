#include "storage.hpp"
#include <dirent.h>
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "Storage";

esp_err_t Storage::init(const std::string& base_path, const std::string& label) {
    ESP_LOGI(TAG, "Mounting SPIFFS at %s...", base_path.c_str());
    
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = base_path.c_str();
    conf.partition_label = label.empty() ? NULL : label.c_str();
    conf.max_files = 5;
    conf.format_if_mount_failed = true;
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(label.empty() ? NULL : label.c_str(), &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}

void Storage::list_files(const std::string& path) {
    ESP_LOGI(TAG, "Listing files in %s:", path.c_str());
    DIR *dir = opendir(path.c_str());
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory %s", path.c_str());
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  Found file: %s", ent->d_name);
    }
    closedir(dir);
}

bool Storage::is_mounted(const std::string& label) {
    return esp_spiffs_mounted(label.empty() ? NULL : label.c_str());
}
