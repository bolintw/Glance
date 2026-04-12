#include "http_client.hpp"
#include "esp_log.h"
#include <cstring>
#include <cinttypes>

static const char *TAG = "HttpClient";

HttpClient::HttpClient() : line_buffer_pos(0) {
    memset(line_buffer, 0, sizeof(line_buffer));
}

HttpClient::~HttpClient() {
}

esp_err_t HttpClient::http_event_handler(esp_http_client_event_t *evt) {
    HttpClient* client = static_cast<HttpClient*>(evt->user_data);
    if (client) {
        return client->handle_event(evt);
    }
    return ESP_OK;
}

esp_err_t HttpClient::handle_event(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data && evt->data_len > 0) {
                process_data_chunk(static_cast<const char*>(evt->data), evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            // Flush any remaining data in the buffer that didn't end with a newline
            if (line_buffer_pos > 0) {
                line_buffer[line_buffer_pos] = '\0';
                ESP_LOGI(TAG, "%s", line_buffer);
                line_buffer_pos = 0;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void HttpClient::process_data_chunk(const char* data, int len) {
    for (int i = 0; i < len; ++i) {
        char c = data[i];
        if (c == '\n') {
            // Null-terminate the string
            line_buffer[line_buffer_pos] = '\0';
            
            // Remove carriage return if present
            if (line_buffer_pos > 0 && line_buffer[line_buffer_pos - 1] == '\r') {
                line_buffer[line_buffer_pos - 1] = '\0';
            }
            
            // Log the complete line
            ESP_LOGI(TAG, "%s", line_buffer);
            
            // Reset buffer position for the next line
            line_buffer_pos = 0;
        } else {
            if (line_buffer_pos < LINE_BUFFER_SIZE - 1) {
                line_buffer[line_buffer_pos++] = c;
            } else {
                // Buffer is full without encountering a newline.
                // Force flush the current buffer content.
                line_buffer[LINE_BUFFER_SIZE - 1] = '\0';
                ESP_LOGI(TAG, "%s", line_buffer);
                
                // Start a new line with the current character
                line_buffer_pos = 0;
                line_buffer[line_buffer_pos++] = c;
            }
        }
    }
}

esp_err_t HttpClient::fetch(const std::string& url) {
    // Reset buffer state before starting a new request
    line_buffer_pos = 0; 
    
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.event_handler = http_event_handler;
    config.user_data = this;
    config.buffer_size = 2048; // Set adequate internal buffer
    config.timeout_ms = 10000;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Fetching URL: %s", url.c_str());
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP request completed. Status = %d, content_length = %" PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
