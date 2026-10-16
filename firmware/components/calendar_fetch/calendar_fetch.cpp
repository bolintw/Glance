#include "calendar_fetch.hpp"

#include <memory>
#include <type_traits>

#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace {
constexpr const char* kTag = "calendar_fetch";
constexpr int kTimeoutMs = 15000;  // for each network operation
// For the whole download: kTimeoutMs alone doesn't end a server that trickles
// data, and one stuck calendar shouldn't hold up the rest.
constexpr int64_t kMaxDownloadUs = 60 * 1000 * 1000;
constexpr int kReadChunkBytes = 1024;

// esp_http_client_cleanup() also closes the connection.
using HttpClient = std::unique_ptr<std::remove_pointer_t<esp_http_client_handle_t>, decltype(&esp_http_client_cleanup)>;
}  // namespace

namespace calendar_fetch {

esp_err_t fetchLines(const char* url, const ics::LineReader::LineHandler& onLine) {
    const int64_t startUs = esp_timer_get_time();
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = kTimeoutMs;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    HttpClient client(esp_http_client_init(&config), &esp_http_client_cleanup);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_open(client.get(), 0);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "open failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_http_client_fetch_headers(client.get());
    int status = esp_http_client_get_status_code(client.get());
    if (status != 200) {
        ESP_LOGE(kTag, "HTTP status %d", status);
        return ESP_FAIL;
    }

    const int64_t connectedUs = esp_timer_get_time();  // TLS handshake and headers done
    ics::LineReader reader(onLine);
    char buf[kReadChunkBytes];
    int totalBytes = 0;
    int n;
    const int64_t deadline = esp_timer_get_time() + kMaxDownloadUs;
    while ((n = esp_http_client_read(client.get(), buf, sizeof(buf))) > 0) {
        reader.feed(std::string_view(buf, static_cast<size_t>(n)));
        totalBytes += n;
        if (esp_timer_get_time() > deadline) {
            ESP_LOGE(kTag, "gave up after %lld s and %d bytes", kMaxDownloadUs / 1000000, totalBytes);
            return ESP_ERR_TIMEOUT;
        }
    }
    reader.finish();

    if (n < 0) {
        ESP_LOGE(kTag, "read failed after %d bytes", totalBytes);
        return ESP_FAIL;
    }
    const int64_t doneUs = esp_timer_get_time();
    ESP_LOGI(kTag, "received %d bytes: connecting %lld ms, reading %lld ms", totalBytes,
             (connectedUs - startUs) / 1000, (doneUs - connectedUs) / 1000);
    return ESP_OK;
}

}  // namespace calendar_fetch
