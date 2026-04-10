#pragma once
#include <string>
#include <vector>
#include "esp_err.h"

struct WifiConfig {
    std::string ssid;
    std::string password;
};

struct CalendarConfig {
    std::vector<std::string> urls;
};

struct AppConfig {
    WifiConfig wifi;
    CalendarConfig calendar;
};

class ConfigManager {
public:
    ConfigManager(const std::string& path);
    esp_err_t load();

    const AppConfig& get() const { return config; }

private:
    std::string file_path;
    AppConfig config;
};
