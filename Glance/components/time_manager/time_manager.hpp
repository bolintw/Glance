#pragma once

#include <string>
#include "esp_err.h"

class TimeManager {
public:
    static TimeManager& get_instance() {
        static TimeManager instance;
        return instance;
    }

    // 初始化 NTP 並同步時間
    esp_err_t sync();
    
    // 設定時區 (例如 "CST-8" 代表台北)
    void set_timezone(const std::string& tz);

    // 取得目前時間字串 (格式化輸出)
    std::string get_formatted_time();

    // 檢查時間是否已經校準
    bool is_time_set();

private:
    TimeManager() = default;
};
