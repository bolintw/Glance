#pragma once

#include <string>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"

class Wifi {
public:
    static Wifi& get_instance() {
        static Wifi instance;
        return instance;
    }

    esp_err_t connect(const std::string& ssid, const std::string& password);
    esp_err_t disconnect();
    bool is_connected() const { return connected; }

private:
    Wifi();
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);
    
    bool connected = false;
    uint8_t retry_num = 0;
};
