#include "settings.hpp"

#include <vector>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#if CONFIG_NVS_ENCRYPTION
#include "esp_efuse.h"
#include "esp_ota_ops.h"
#endif

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
constexpr const char* kPanelFlippedKey = "upside_down";  // u8, Settings::panelFlipped
constexpr const char* kThemeKey = "theme";             // u8, Settings::Theme
constexpr const char* kPhotoAutoThemeKey = "photo_auto";  // u8
constexpr const char* kRefreshKey = "refresh_1";       // u32, minutes after midnight
constexpr const char* kSecondRefreshKey = "refresh_2";  // u32, minutes after midnight; none if out of range
constexpr const char* kCornerQrKey = "guest_wifi";    // u8, Settings::CornerQr
constexpr const char* kCornerTextKey = "qr_text";
constexpr const char* kGuestSsidKey = "guest_ssid";
constexpr const char* kGuestPasswordKey = "guest_pass";
constexpr const char* kPhotoNextKey = "photo_next";    // u8, 0xFF = none
constexpr const char* kPhotoShownKey = "photo_shown";  // u32 bitmask
constexpr const char* kWorkingSsidKey = "wifi_ok_ssid";
constexpr const char* kWorkingPasswordKey = "wifi_ok_pass";
constexpr const char* kWifiRevertKey = "wifi_reverted";  // the network that didn't connect
constexpr const char* kCalendarFailuresKey = "cal_fail";  // blob: uint32 per slot

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
    bool readBlob(const char* key, void* out, size_t size) const {
        size_t length = size;
        return open_ && nvs_get_blob(handle_, key, out, &length) == ESP_OK && length == size;
    }
    esp_err_t writeBlob(const char* key, const void* data, size_t size) { return nvs_set_blob(handle_, key, data, size); }
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

namespace {

// Initializes the default partition with `init` (encrypted or not), erasing
// it first if its format is unusable.
esp_err_t initOrErase(esp_err_t (*init)()) {
    esp_err_t err = init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "NVS unusable (%s), erasing", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = init();
        }
    }
    return err;
}

esp_err_t initPlaintext() { return nvs_flash_init_partition(NVS_DEFAULT_PART_NAME); }

#if CONFIG_NVS_ENCRYPTION
// Encrypted, falling back to plaintext if that fails (an eFuse that won't
// take the key, say): settings that work beat settings that are safe.
esp_err_t initEncrypted() {
    esp_err_t err = initOrErase(nvs_flash_init);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "encrypted NVS failed (%s), going on in plaintext", esp_err_to_name(err));
        err = initOrErase(initPlaintext);
    }
    return err;
}

// The first encrypted init burns the key, so until then NVS is plaintext.
bool encryptionKeyBurnt() {
    auto block = static_cast<esp_efuse_block_t>(EFUSE_BLK_KEY0 + CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID);
    return esp_efuse_get_key_purpose(block) == ESP_EFUSE_KEY_PURPOSE_HMAC_UP;
}

bool onProbation() {
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

// One entry of this namespace, held while NVS is rebuilt.
struct SavedEntry {
    std::string key;
    nvs_type_t type;
    uint32_t number = 0;         // U8, U32
    std::vector<uint8_t> bytes;  // STR (with its NUL), BLOB
};

std::vector<SavedEntry> readNamespace() {
    std::vector<SavedEntry> saved;
    Nvs nvs(NVS_READONLY);
    if (!nvs.ok()) {
        return saved;  // never written
    }
    nvs_iterator_t it = nullptr;
    for (esp_err_t res = nvs_entry_find(NVS_DEFAULT_PART_NAME, kNamespace, NVS_TYPE_ANY, &it); res == ESP_OK;
         res = nvs_entry_next(&it)) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        SavedEntry e{info.key, info.type, 0, {}};
        size_t length = 0;
        esp_err_t err = ESP_FAIL;
        switch (info.type) {
            case NVS_TYPE_U8: {
                uint8_t v;
                err = nvs_get_u8(nvs.handle(), info.key, &v);
                e.number = v;
                break;
            }
            case NVS_TYPE_U32:
                err = nvs_get_u32(nvs.handle(), info.key, &e.number);
                break;
            case NVS_TYPE_STR:
                if (nvs_get_str(nvs.handle(), info.key, nullptr, &length) == ESP_OK) {
                    e.bytes.resize(length);
                    err = nvs_get_str(nvs.handle(), info.key, reinterpret_cast<char*>(e.bytes.data()), &length);
                }
                break;
            case NVS_TYPE_BLOB:
                if (nvs_get_blob(nvs.handle(), info.key, nullptr, &length) == ESP_OK) {
                    e.bytes.resize(length);
                    err = nvs_get_blob(nvs.handle(), info.key, e.bytes.data(), &length);
                }
                break;
            default:
                ESP_LOGW(kTag, "not moving %s: type 0x%02x", info.key, info.type);  // none of ours
                break;
        }
        if (err == ESP_OK) {
            saved.push_back(std::move(e));
        }
    }
    nvs_release_iterator(it);
    return saved;
}

void writeNamespace(const std::vector<SavedEntry>& saved) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        ESP_LOGE(kTag, "couldn't write the settings back");
        return;
    }
    for (const SavedEntry& e : saved) {
        const char* key = e.key.c_str();
        esp_err_t err = ESP_FAIL;
        switch (e.type) {
            case NVS_TYPE_U8: err = nvs.writeU8(key, static_cast<uint8_t>(e.number)); break;
            case NVS_TYPE_U32: err = nvs.writeU32(key, e.number); break;
            case NVS_TYPE_STR: err = nvs_set_str(nvs.handle(), key, reinterpret_cast<const char*>(e.bytes.data())); break;
            case NVS_TYPE_BLOB: err = nvs.writeBlob(key, e.bytes.data(), e.bytes.size()); break;
            default: break;
        }
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "couldn't write %s back: %s", key, esp_err_to_name(err));
        }
    }
    nvs.commit();
}

// Reads this namespace in plaintext, erases the partition in flash (the
// plaintext copies go, and the WiFi driver's, which it rewrites), and
// writes the entries back encrypted.
esp_err_t moveToEncrypted() {
    std::vector<SavedEntry> saved;
    if (initOrErase(initPlaintext) == ESP_OK) {
        saved = readNamespace();
        nvs_flash_deinit();
    }
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "couldn't erase NVS to encrypt it (%s), staying in plaintext", esp_err_to_name(err));
        return initOrErase(initPlaintext);
    }
    err = initEncrypted();
    if (err == ESP_OK) {
        writeNamespace(saved);
        ESP_LOGW(kTag, "settings moved to encrypted NVS: %u entries", static_cast<unsigned>(saved.size()));
    }
    return err;
}
#endif

}  // namespace

esp_err_t initStorage() {
    esp_err_t err;
#if CONFIG_NVS_ENCRYPTION
    if (encryptionKeyBurnt()) {
        err = initEncrypted();
    } else if (onProbation()) {
        ESP_LOGI(kTag, "fresh update: NVS stays plaintext until it has proven itself");
        err = initOrErase(initPlaintext);
    } else {
        err = moveToEncrypted();
    }
#else
    err = initOrErase(initPlaintext);
#endif
    if (err != ESP_OK) {
        // Nothing to lose by now: an empty store sends the device to setup.
        ESP_LOGE(kTag, "NVS unusable (%s), starting it over", esp_err_to_name(err));
        nvs_flash_erase();
        err = initOrErase(initPlaintext);
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
    s.panelFlipped = nvs.readU8(kPanelFlippedKey, 1) != 0;
    s.theme = nvs.readU8(kThemeKey, 0) == 1 ? Settings::Theme::light : Settings::Theme::dark;
    s.photoAutoTheme = nvs.readU8(kPhotoAutoThemeKey, 1) != 0;
    constexpr uint32_t kDay = 24 * 60;
    uint32_t first = nvs.readU32(kRefreshKey, CONFIG_GLANCE_REFRESH_HOUR * 60);
    s.refreshMinutes = static_cast<int>(first < kDay ? first : CONFIG_GLANCE_REFRESH_HOUR * 60);
    uint32_t second = nvs.readU32(kSecondRefreshKey, UINT32_MAX);
    s.secondRefreshMinutes = second < kDay ? static_cast<int>(second) : Settings::kNoRefresh;
    uint8_t guest = nvs.readU8(kCornerQrKey, 0);
    s.cornerQr = guest <= static_cast<uint8_t>(Settings::CornerQr::text) ? static_cast<Settings::CornerQr>(guest)
                                                                          : Settings::CornerQr::off;
    s.guestSsid = nvs.read(kGuestSsidKey, "");
    s.guestPassword = nvs.read(kGuestPasswordKey, "");
    s.cornerText = nvs.read(kCornerTextKey, "");
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
    write(kGuestSsidKey, s.guestSsid);
    write(kGuestPasswordKey, s.guestPassword);
    write(kCornerTextKey, s.cornerText);
    if (err == ESP_OK) {
        err = nvs.writeU8(kPanelFlippedKey, s.panelFlipped ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs.writeU8(kCornerQrKey, static_cast<uint8_t>(s.cornerQr));
    }
    if (err == ESP_OK) {
        err = nvs.writeU8(kThemeKey, static_cast<uint8_t>(s.theme));
    }
    if (err == ESP_OK) {
        err = nvs.writeU8(kPhotoAutoThemeKey, s.photoAutoTheme ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs.writeU32(kRefreshKey, static_cast<uint32_t>(s.refreshMinutes));
    }
    if (err == ESP_OK) {
        err = nvs.writeU32(kSecondRefreshKey, s.secondRefreshMinutes == Settings::kNoRefresh
                                                  ? UINT32_MAX
                                                  : static_cast<uint32_t>(s.secondRefreshMinutes));
    }
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
    nvs_flash_deinit();
    esp_err_t err = nvs_flash_erase();
    ESP_LOGW(kTag, "NVS erased: %s", esp_err_to_name(err));
    esp_err_t init = initStorage();
    return err != ESP_OK ? err : init;
}

CalendarFailures loadCalendarFailures() {
    CalendarFailures failures{};
    if (!Nvs(NVS_READONLY).readBlob(kCalendarFailuresKey, failures.data(), sizeof(failures))) {
        failures.fill(0);
    }
    return failures;
}

esp_err_t saveCalendarFailures(const CalendarFailures& failures) {
    Nvs nvs(NVS_READWRITE);
    if (!nvs.ok()) {
        return ESP_FAIL;
    }
    esp_err_t err = nvs.writeBlob(kCalendarFailuresKey, failures.data(), sizeof(failures));
    return err == ESP_OK ? nvs.commit() : err;
}

bool loadPanelFlipped() { return Nvs(NVS_READONLY).readU8(kPanelFlippedKey, 1) != 0; }

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
