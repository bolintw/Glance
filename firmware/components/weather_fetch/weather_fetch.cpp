#include "weather_fetch.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace {
constexpr const char* kTag = "weather_fetch";
constexpr const char* kEndpoint = "https://opendata.cwa.gov.tw/api/v1/rest/datastore/F-C0032-001";
constexpr int kTimeoutMs = 15000;
// A single county's response is ~2KB; anything far bigger isn't what we asked for.
constexpr size_t kMaxResponseBytes = 16 * 1024;

using HttpClient = std::unique_ptr<std::remove_pointer_t<esp_http_client_handle_t>, decltype(&esp_http_client_cleanup)>;

// RFC 3986 percent-encoding for a query value (the county name is UTF-8).
void appendEncoded(std::string& out, std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    for (unsigned char c : value) {
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                          c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
}

std::optional<std::string> fetchBody(const std::string& url) {
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = kTimeoutMs;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    HttpClient client(esp_http_client_init(&config), &esp_http_client_cleanup);
    if (!client) {
        return std::nullopt;
    }
    esp_err_t err = esp_http_client_open(client.get(), 0);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "open failed: %s", esp_err_to_name(err));
        return std::nullopt;
    }
    esp_http_client_fetch_headers(client.get());
    int status = esp_http_client_get_status_code(client.get());
    if (status != 200) {
        ESP_LOGE(kTag, "HTTP status %d", status);  // 401: wrong API key
        return std::nullopt;
    }

    std::string body;
    char buf[1024];
    int n;
    while ((n = esp_http_client_read(client.get(), buf, sizeof(buf))) > 0) {
        if (body.size() + static_cast<size_t>(n) > kMaxResponseBytes) {
            ESP_LOGE(kTag, "response over %zu bytes", kMaxResponseBytes);
            return std::nullopt;
        }
        body.append(buf, static_cast<size_t>(n));
    }
    if (n < 0) {
        ESP_LOGE(kTag, "read failed after %zu bytes", body.size());
        return std::nullopt;
    }
    return body;
}

const char* conditionName(weather::Condition condition) {
    switch (condition) {
        case weather::Condition::sunny: return "sunny";
        case weather::Condition::partlyCloudy: return "partly cloudy";
        case weather::Condition::cloudy: return "cloudy";
        case weather::Condition::windy: return "windy";
        case weather::Condition::rainy: return "rainy";
    }
    return "?";
}
}  // namespace

namespace weather_fetch {

std::optional<weather::Forecast> fetchForecast(int64_t now, const std::string& apiKey, const std::string& location) {
    if (apiKey.empty() || location.empty()) {
        ESP_LOGI(kTag, "weather not configured, skipping");
        return std::nullopt;
    }

    std::string url = kEndpoint;
    url += "?Authorization=";
    appendEncoded(url, apiKey);
    url += "&locationName=";
    appendEncoded(url, location);
    url += "&elementName=Wx,PoP,MinT,MaxT";

    auto body = fetchBody(url);
    if (!body) {
        return std::nullopt;
    }
    auto forecast = weather::parseCwa36Hour(*body, now);
    if (!forecast) {
        ESP_LOGE(kTag, "couldn't read the forecast from %zu bytes", body->size());
        return std::nullopt;
    }
    ESP_LOGI(kTag, "%s (%s), %d-%d C, %d%% rain", forecast->description.c_str(), conditionName(forecast->condition),
             forecast->minTemp, forecast->maxTemp, forecast->rainChance);
    return forecast;
}

}  // namespace weather_fetch
