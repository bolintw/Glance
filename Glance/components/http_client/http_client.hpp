#pragma once

#include <string>
#include "esp_err.h"
#include "esp_http_client.h"

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Fetch data from the specified URL
    esp_err_t fetch(const std::string& url);

private:
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
    esp_err_t handle_event(esp_http_client_event_t *evt);

    void process_data_chunk(const char* data, int len);

    static constexpr size_t LINE_BUFFER_SIZE = 512;
    char line_buffer[LINE_BUFFER_SIZE];
    size_t line_buffer_pos;
};
