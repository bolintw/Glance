#pragma once

#include <string>
#include "esp_err.h"

class TimeManager {
public:
    static TimeManager& get_instance() {
        static TimeManager instance;
        return instance;
    }

    // Initialize NTP and synchronize time
    esp_err_t sync();
    
    // Set timezone (e.g., "CST-8" for Taipei)
    void set_timezone(const std::string& tz);

    // Get current time string (formatted output)
    std::string get_formatted_time();

    // Check if system time has been set
    bool is_time_set();

private:
    TimeManager() = default;
};
