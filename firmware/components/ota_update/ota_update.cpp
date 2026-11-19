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
#include "nvs.h"
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

bool onProbation() {
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

namespace {
// The manifest, read; reading it is what ends a fresh update's probation.
std::optional<ota_manifest::Manifest> readManifest(const std::string& manifestUrl) {
    auto body = fetchManifest(manifestUrl);
    if (!body) {
        return std::nullopt;
    }
    auto manifest = ota_manifest::parseManifest(*body);
    if (!manifest) {
        ESP_LOGW(kTag, "manifest unreadable (%zu bytes)", body->size());
        return std::nullopt;
    }
    if (onProbation()) {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(kTag, "update %s accepted: it reaches the next one", runningVersion());
    }
    return manifest;
}

// How many times each update was installed (see ota_manifest::kMaxAttempts):
// the last version installed and its count, in their own NVS namespace.
constexpr const char* kNvsNamespace = "ota_update";
constexpr const char* kAttemptVersionKey = "try_ver";
constexpr const char* kAttemptCountKey = "try_n";

int attempts(const std::string& version) {
    nvs_handle_t nvs;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &nvs) != ESP_OK) {
        return 0;
    }
    char stored[32] = {};
    size_t length = sizeof(stored);
    uint8_t count = 0;
    bool same = nvs_get_str(nvs, kAttemptVersionKey, stored, &length) == ESP_OK && version == stored;
    if (!same || nvs_get_u8(nvs, kAttemptCountKey, &count) != ESP_OK) {
        count = 0;
    }
    nvs_close(nvs);
    return count;
}

void countAttempt(const std::string& version) {
    const int count = attempts(version) + 1;
    nvs_handle_t nvs;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_set_str(nvs, kAttemptVersionKey, version.c_str());
    nvs_set_u8(nvs, kAttemptCountKey, static_cast<uint8_t>(count > 255 ? 255 : count));
    nvs_commit(nvs);
    nvs_close(nvs);
}

// Automatic updates (`byHand` false) give up on a version that rolled back
// kMaxAttempts times; one asked for by hand is always tried.
bool installable(const ota_manifest::Manifest& manifest, bool byHand) {
    std::string failed = lastFailedVersion();
    const int tried = byHand ? 0 : attempts(manifest.version);
    if (!ota_manifest::shouldInstall(runningVersion(), manifest, failed, tried)) {
        ESP_LOGI(kTag, "running %s, latest %s%s%s: nothing to do", runningVersion(), manifest.version.c_str(),
                 failed.empty() ? "" : ", gave up on ", failed.c_str());
        return false;
    }
    return true;
}

// Downloads and installs, then restarts into it; returns only on failure.
void install(const ota_manifest::Manifest& manifest) {
    ESP_LOGI(kTag, "installing %s (running %s)", manifest.version.c_str(), runningVersion());
    countAttempt(manifest.version);
    esp_http_client_config_t http = httpConfig(manifest.url.c_str());
    esp_https_ota_config_t ota = {};
    ota.http_config = &http;
    esp_err_t err = esp_https_ota(&ota);  // downloads, verifies the image, sets it to boot next
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "update failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "installed %s, restarting", manifest.version.c_str());
    esp_restart();
}
}  // namespace

void checkAndInstall(const std::string& manifestUrl, const char* skipReason) {
    if (!ota_manifest::parseVersion(runningVersion())) {
        ESP_LOGI(kTag, "development build (%s), not checking for updates", runningVersion());
        return;
    }
    auto manifest = readManifest(manifestUrl);
    if (!manifest) {
        return;
    }
    if (skipReason) {
        ESP_LOGI(kTag, "%s: latest is %s, not installing", skipReason, manifest->version.c_str());
        return;
    }
    if (installable(*manifest, false)) {
        install(*manifest);
    }
}

std::optional<Latest> checkLatest(const std::string& manifestUrl) {
    auto manifest = readManifest(manifestUrl);
    if (!manifest) {
        return std::nullopt;
    }
    return Latest{manifest->version, installable(*manifest, true)};
}

bool installLatest(const std::string& manifestUrl) {
    auto manifest = readManifest(manifestUrl);
    if (!manifest || !installable(*manifest, true)) {
        return false;
    }
    install(*manifest);
    return false;
}

}  // namespace ota_update
