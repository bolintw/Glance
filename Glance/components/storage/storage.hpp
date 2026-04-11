#pragma once

#include <string>
#include "esp_err.h"

class Storage {
public:
    // Initialize and mount SPIFFS
    static esp_err_t init(const std::string& base_path = "/spiffs", 
                         const std::string& label = "");

    // List files in the directory (for debugging)
    static void list_files(const std::string& path = "/spiffs");

    // Check if the filesystem is mounted
    static bool is_mounted(const std::string& label = "");

private:
    Storage() = default;
};
