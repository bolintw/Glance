#include "settings.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {
constexpr const char* kTag = "settings";
constexpr const char* kNamespace = "glance";

// NVS keys are at most 15 characters.
constexpr const char* kWifiSsidKey = "wifi_ssid";
constexpr const char* kWifiPasswordKey = "wifi_pass";
constexpr const char* kIcsUrlKeys[Settings::kMaxCalendars] = {"ics_url_1", "ics_url_2", "ics_url_3", "ics_url_4",
                                                              "ics_url_5"};
constexpr const char* kCwaApiKeyKey = "cwa_key";
constexpr const char* kWeatherLocationKey = "wx_location";
constexpr const char* kPrivacyModeKey = "privacy";
constexpr const char* kPhotoNextKey = "photo_next";    // u8, 0xFF = none
constexpr const char* kPhotoShownKey = "photo_shown";  // u32 bitmask
constexpr const char* kWorkingSsidKey = "wifi_ok_ssid";
constexpr const char* kWorkingPasswordKey = "wifi_ok_pass";
constexpr const char* kWifiRevertKey = "wifi_reverted";  // the network that didn't connect

constexpr const char* kIcsUrlFallbacks[Settings::kMaxCalendars] = {
    CONFIG_GLANCE_ICS_URL_1, CONFIG_GLANCE_ICS_URL_2, CONFIG_GLANCE_ICS_URL_3,
    CONFIG_GLANCE_ICS_URL_4, CONFIG_GLANCE_ICS_URL_5,
};

// One open NVS namespace, closed on scope exit. Not open (ok() false) until
// the namespace has been written at least once, for read-only access.
class Nvs {
public:
    explicit Nvs(nvs_open_mode_t mode) : open_(nvs_open(kNamespace, mode, &handle_) == ESP_OK) {}
    ~Nvs() {
        if (open_) {
            nvs_close(handle_);
        }
    }
    Nvs(const Nvs&) = delete;
    Nvs& operator=(const Nvs&) = delete;

    bool ok() const { return open_; }

    std::string read(const char* key, const char* fallback) const {
        size_t length = 0;
        if (!open_ || nvs_get_str(handle_, key, nullptr, &length) != ESP_OK) {
            return fallback;
        }
        std::string value(length, '\0');  // length includes the terminating NUL
        if (nvs_get_str(handle_, key, value.data(), &length) != ESP_OK) {
            return fallback;
        }
        value.resize(length - 1);
        return value;
    }

    esp_err_t write(const char* key, const std::string& value) { return nvs_set_str(handle_, key, value.c_str()); }

    uint8_t readU8(const char* key, uint8_t fallback) const {
        uint8_t value;
        return open_ && nvs_get_u8(handle_, key, &value) == ESP_OK ? value : fallback;
    }
    esp_err_t writeU8(const char* key, uint8_t value) { return nvs_set_u8(handle_, key, value); }

    uint32_t readU32(const char* key, uint32_t fallback) const {
        uint32_t value;
        return open_ && nvs_get_u32(handle_, key, &value) == ESP_OK ? value : fallback;
    }
    esp_err_t writeU32(const char* key, uint32_t value) { return nvs_set_u32(handle_, key, value); }
    esp_err_t commit() { return nvs_commit(handle_); }
    nvs_handle_t handle() const { return handle_; }
    esp_err_t erase(const char* key) {
        esp_err_t err = nvs_erase_key(handle_, key);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }

private:
    nvs_handle_t handle_ = 0;
    bool open_;
};
}  // namespace

namespace settings {

esp_err_t initStorage() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "NVS unusable (%s), erasing", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

Settings load() {
    Nvs nvs(NVS_READONLY);
    Settings s;
    s.wifiSsid = nvs.read(kWifiSsidKey, CONFIG_GLANCE_WIFI_SSID);
    s.wifiPassword = nvs.read(kWifiPasswordKey, CONFIG_GLANCE_WIFI_PASSWORD);
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        s.icsUrls[i] = nvs.read(kIcsUrlKeys[i], kIcsUrlFallbacks[i]);
    }
    s.cwaApiKey = nvs.read(kCwaApiKeyKey, CONFIG_GLANCE_CWA_API_KEY);
    s.weatherLocation = nvs.read(kWeatherLocationKey, CONFIG_GLANCE_WEATHER_LOCATION);
    ESP_LOGI(kTag, "loaded (%s)", nvs.ok() ? "device settings" : "menuconfig fallback");
    return s;
}

esp_err_t save(const Settings& s) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;
    auto write = [&](const char* key, const std::string& value) {
        if (err == ESP_OK) {
            err = nvs.write(key, value);
        }
    };
    write(kWifiSsidKey, s.wifiSsid);
    write(kWifiPasswordKey, s.wifiPassword);
    for (size_t i = 0; i < Settings::kMaxCalendars; i++) {
        write(kIcsUrlKeys[i], s.icsUrls[i]);
    }
    write(kCwaApiKeyKey, s.cwaApiKey);
    write(kWeatherLocationKey, s.weatherLocation);
    if (err == ESP_OK) {
        err = nvs.commit();
    }
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t forgetWifi() {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.erase(kWifiSsidKey);
    if (err == ESP_OK) {
        err = nvs.erase(kWifiPasswordKey);
    }
    return err == ESP_OK ? nvs.commit() : err;
}

std::optional<WifiNetwork> loadWorkingWifi() {
    Nvs nvs(NVS_READONLY);
    WifiNetwork network{nvs.read(kWorkingSsidKey, ""), nvs.read(kWorkingPasswordKey, "")};
    if (network.ssid.empty()) {
        return std::nullopt;
    }
    return network;
}

esp_err_t rememberWorkingWifi(const WifiNetwork& network) {
    if (auto known = loadWorkingWifi(); known && known->ssid == network.ssid && known->password == network.password) {
        return ESP_OK;
    }
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.write(kWorkingSsidKey, network.ssid);
    if (err == ESP_OK) {
        err = nvs.write(kWorkingPasswordKey, network.password);
    }
    return err == ESP_OK ? nvs.commit() : err;
}

esp_err_t revertWifi(const std::string& failedSsid, const WifiNetwork& working) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.write(kWifiSsidKey, working.ssid);
    if (err == ESP_OK) {
        err = nvs.write(kWifiPasswordKey, working.password);
    }
    if (err == ESP_OK) {
        err = nvs.write(kWifiRevertKey, failedSsid);
    }
    return err == ESP_OK ? nvs.commit() : err;
}

std::string takeWifiRevertNote() {
    std::string note = Nvs(NVS_READONLY).read(kWifiRevertKey, "");
    if (!note.empty()) {
        Nvs nvs(NVS_READWRITE);
        if (nvs.ok() && nvs.erase(kWifiRevertKey) == ESP_OK) {
            nvs.commit();
        }
    }
    return note;
}

esp_err_t eraseAll() {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs_erase_all(nvs.handle());
    if (err == ESP_OK) {
        err = nvs.commit();
    }
    ESP_LOGW(kTag, "all settings erased: %s", esp_err_to_name(err));
    return err;
}

bool loadPrivacyMode() { return Nvs(NVS_READONLY).readU8(kPrivacyModeKey, 0) != 0; }

esp_err_t savePrivacyMode(bool on) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.writeU8(kPrivacyModeKey, on ? 1 : 0);
    return err == ESP_OK ? nvs.commit() : err;
}

photo_rotation::State loadPhotoRotation() {
    Nvs nvs(NVS_READONLY);
    uint8_t next = nvs.readU8(kPhotoNextKey, 0xFF);
    return {next == 0xFF ? -1 : next, nvs.readU32(kPhotoShownKey, 0)};
}

esp_err_t savePhotoRotation(const photo_rotation::State& state) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.writeU8(kPhotoNextKey, state.showNext < 0 ? 0xFF : static_cast<uint8_t>(state.showNext));
    if (err == ESP_OK) {
        err = nvs.writeU32(kPhotoShownKey, state.shown);
    }
    return err == ESP_OK ? nvs.commit() : err;
}

}  // namespace settings
