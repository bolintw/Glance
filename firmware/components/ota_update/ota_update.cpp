#include "ota_update.hpp"

#include <optional>
#include <string>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "ota_manifest.hpp"

namespace {
constexpr const char* kTag = "ota_update";
constexpr int kTimeoutMs = 15000;
constexpr size_t kMaxManifestBytes = 2048;
// GitHub answers release downloads with a redirect whose Location header is
// a long signed URL; the default 512-byte buffers can't hold it.
constexpr int kHttpBufferBytes = 4096;

esp_err_t collectBody(esp_http_client_event_t* event) {
    auto* body = static_cast<std::string*>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_DATA && body->size() + event->data_len <= kMaxManifestBytes) {
        body->append(static_cast<const char*>(event->data), static_cast<size_t>(event->data_len));
    }
    return ESP_OK;
}

esp_http_client_config_t httpConfig(const char* url) {
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = kTimeoutMs;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = kHttpBufferBytes;
    config.buffer_size_tx = kHttpBufferBytes;
    config.keep_alive_enable = true;
    return config;
}

std::optional<std::string> fetchManifest(const std::string& url) {
    std::string body;
    esp_http_client_config_t config = httpConfig(url.c_str());
    config.event_handler = collectBody;
    config.user_data = &body;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return std::nullopt;
    }
    esp_err_t err = esp_http_client_perform(client);  // follows redirects
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status != 200) {
        ESP_LOGW(kTag, "manifest fetch failed: %s, HTTP %d", esp_err_to_name(err), status);
        return std::nullopt;
    }
    return body;
}

// The version a previous update tried and rolled back from, if any.
std::string lastFailedVersion() {
    const esp_partition_t* invalid = esp_ota_get_last_invalid_partition();
    esp_app_desc_t desc;
    if (!invalid || esp_ota_get_partition_description(invalid, &desc) != ESP_OK) {
        return {};
    }
    return desc.version;
}
}  // namespace

namespace ota_update {

const char* runningVersion() { return esp_app_get_description()->version; }

void markRunningAppValid() {
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(kTag, "update %s accepted", runningVersion());
    }
}

void checkAndInstall(const std::string& manifestUrl) {
    if (!ota_manifest::parseVersion(runningVersion())) {
        ESP_LOGI(kTag, "development build (%s), not checking for updates", runningVersion());
        return;
    }
    auto body = fetchManifest(manifestUrl);
    if (!body) {
        return;
    }
    auto manifest = ota_manifest::parseManifest(*body);
    if (!manifest) {
        ESP_LOGW(kTag, "manifest unreadable (%zu bytes)", body->size());
        return;
    }
    std::string failed = lastFailedVersion();
    if (!ota_manifest::shouldInstall(runningVersion(), *manifest, failed)) {
        ESP_LOGI(kTag, "running %s, latest %s%s%s: nothing to do", runningVersion(), manifest->version.c_str(),
                 failed.empty() ? "" : ", rolled back from ", failed.c_str());
        return;
    }

    ESP_LOGI(kTag, "installing %s (running %s)", manifest->version.c_str(), runningVersion());
    esp_http_client_config_t http = httpConfig(manifest->url.c_str());
    esp_https_ota_config_t ota = {};
    ota.http_config = &http;
    esp_err_t err = esp_https_ota(&ota);  // downloads, verifies the image, sets it to boot next
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "update failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "installed %s, restarting", manifest->version.c_str());
    esp_restart();
}

}  // namespace ota_update
