#include "setup_mode.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <optional>
#include <string>
#include <cstring>
#include <vector>

#include "captive_dns.hpp"
#include "lan_access.hpp"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "photo_format.hpp"
#include "photo_store.hpp"
#include "photos.hpp"
#include "settings.hpp"
#include "setup_page.hpp"
#include "station_watch.hpp"

namespace {
constexpr const char* kTag = "setup_mode";
constexpr std::array<uint8_t, 4> kApIp = {192, 168, 4, 1};  // ESP-IDF's default AP address
constexpr const char* kPageUrl = "http://192.168.4.1/";
constexpr size_t kMaxNearby = 15;
constexpr size_t kMaxFormBytes = 8 * 1024;
constexpr size_t kMaxPhotoBytes = 128 * 1024;  // one upload is ~75KB
constexpr int kSavedBit = BIT0;
constexpr int kJoinedBit = BIT1;    // first station joined: take the QR code down
constexpr int kIntruderBit = BIT2;  // a second device got in: close up
constexpr int kErasedBit = BIT3;
constexpr const char* kSessionCookie = "glance_session";

constexpr const char* kDeniedPage = R"(<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><link rel="icon" href="data:,">
<title>Glance</title></head><body style="font-family:system-ui,sans-serif;padding:1rem">
<h1>請掃描裝置上的 QR code</h1><p>設定頁只能從裝置螢幕上的 QR code 開啟。</p></body></html>
)";

// What this setup session serves (see setup_mode.hpp).
enum class Mode { accessPoint, home, devLan };

// Shared with the HTTP handlers and the DNS task. One setup session per boot.
Mode gMode = Mode::accessPoint;
httpd_handle_t gServer = nullptr;
Settings gCurrent;
std::vector<std::string> gNearby;
std::vector<std::string> gWifiErrors;  // shown on the WiFi form, e.g. that the last network didn't work
std::string gRevertedFrom;             // a new WiFi that didn't connect (see settings::revertWifi)
std::optional<LanAccess> gAccess;      // home mode's gate
std::string gCookieHeader;             // outlives the response it's set on (handlers run one at a time)
EventGroupHandle_t gEvents = nullptr;
std::atomic<int64_t> gLastActivityUs{0};
StationWatch gStations;  // only touched from the event loop task

void onStationJoined(void*, esp_event_base_t, int32_t, void* data) {
    auto* event = static_cast<wifi_event_ap_staconnected_t*>(data);
    StationWatch::Mac mac;
    std::copy(std::begin(event->mac), std::end(event->mac), mac.begin());
    ESP_LOGI(kTag, "station joined: ..:%02x:%02x:%02x", mac[3], mac[4], mac[5]);
    switch (gStations.onJoined(mac)) {
        case StationWatch::Action::hideQr:
            ESP_LOGI(kTag, "a device joined the access point");
            xEventGroupSetBits(gEvents, kJoinedBit);
            break;
        case StationWatch::Action::shutDown:
            ESP_LOGW(kTag, "a second device joined the access point");
            xEventGroupSetBits(gEvents, kIntruderBit);
            break;
        case StationWatch::Action::none:
            break;
    }
}

void touch() { gLastActivityUs = esp_timer_get_time(); }

std::string randomHex(size_t bytes) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < bytes; i++) {
        uint32_t r = esp_random();
        out += kHex[(r >> 4) & 0xF];
        out += kHex[r & 0xF];
    }
    return out;
}

// The requester's address, as text (any stable form will do for LanAccess).
std::string clientAddress(httpd_req_t* req) {
    sockaddr_storage addr = {};
    socklen_t length = sizeof(addr);
    char text[64] = "?";
    if (getpeername(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr*>(&addr), &length) == 0) {
        const void* raw = addr.ss_family == AF_INET6
                              ? static_cast<const void*>(&reinterpret_cast<sockaddr_in6*>(&addr)->sin6_addr)
                              : static_cast<const void*>(&reinterpret_cast<sockaddr_in*>(&addr)->sin_addr);
        inet_ntop(addr.ss_family, raw, text, sizeof(text));
    }
    return text;
}

std::vector<std::string> scanNearby() {
    std::vector<std::string> names;
    if (esp_wifi_scan_start(nullptr, true) != ESP_OK) {
        return names;
    }
    uint16_t count = 20;
    std::vector<wifi_ap_record_t> records(count);
    if (esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        return names;
    }
    for (uint16_t i = 0; i < count && names.size() < kMaxNearby; i++) {  // strongest first
        std::string name(reinterpret_cast<const char*>(records[i].ssid));
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
    return names;
}

void dnsTask(void*) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (sock < 0 || bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ESP_LOGE(kTag, "DNS socket failed");
        vTaskDelete(nullptr);
        return;
    }
    uint8_t packet[512];
    while (true) {
        sockaddr_in from = {};
        socklen_t fromLength = sizeof(from);
        int n = recvfrom(sock, packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (n <= 0) {
            continue;
        }
        auto reply = captive_dns::buildReply(std::span<const uint8_t>(packet, static_cast<size_t>(n)), kApIp);
        if (!reply.empty()) {
            sendto(sock, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr*>(&from), fromLength);
        }
    }
}

esp_err_t sendHtml(httpd_req_t* req, const std::string& html) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, html.data(), static_cast<ssize_t>(html.size()));
}

esp_err_t sendText(httpd_req_t* req, const char* status, const char* type, const std::string& text) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, text.data(), static_cast<ssize_t>(text.size()));
}

// Home mode's gate (see LanAccess); the other modes let everyone through.
// When it returns false the request has been answered already.
bool authorize(httpd_req_t* req) {
    if (gMode != Mode::home) {
        return true;
    }
    char cookie[160] = {};
    httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie));  // stays empty if there's none
    LanAccess::Result result = gAccess->check(clientAddress(req), queryParam(req->uri, "t"),
                                              cookieValue(cookie, kSessionCookie), [] { return randomHex(16); });
    switch (result.verdict) {
        case LanAccess::Verdict::allow:
            if (!result.newSession.empty()) {
                gCookieHeader = std::string(kSessionCookie) + "=" + result.newSession + "; Path=/; HttpOnly; SameSite=Strict";
                httpd_resp_set_hdr(req, "Set-Cookie", gCookieHeader.c_str());
            }
            if (result.firstVisit) {
                ESP_LOGI(kTag, "setup page opened from %s", clientAddress(req).c_str());
                xEventGroupSetBits(gEvents, kJoinedBit);
            }
            return true;
        case LanAccess::Verdict::intruder:
            ESP_LOGW(kTag, "a second device (%s) used the setup secret", clientAddress(req).c_str());
            xEventGroupSetBits(gEvents, kIntruderBit);
            break;
        case LanAccess::Verdict::deny:
            break;
    }
    sendText(req, "403 Forbidden", "text/html; charset=utf-8", kDeniedPage);
    return false;
}

esp_err_t handleForm(httpd_req_t* req) {
    touch();
    ESP_LOGI(kTag, "GET %s", gMode == Mode::home ? "/" : req->uri);  // home mode's URI carries the token
    if (!authorize(req)) {
        return ESP_OK;
    }
    return sendHtml(req, gMode == Mode::accessPoint
                             ? setup_page::renderWifiForm(gCurrent, gNearby, gWifiErrors, gRevertedFrom)
                             : setup_page::renderForm(gCurrent, gNearby, {}, gRevertedFrom));
}

// The whole request body, or nullopt (after answering) if it's too big or
// the connection drops.
template <typename Buffer>
std::optional<Buffer> readBody(httpd_req_t* req, size_t maxBytes) {
    if (req->content_len > maxBytes) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "too large");
        return std::nullopt;
    }
    Buffer body(req->content_len, 0);
    size_t received = 0;
    while (received < body.size()) {
        int n = httpd_req_recv(req, reinterpret_cast<char*>(body.data()) + received, body.size() - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            return std::nullopt;
        }
        received += static_cast<size_t>(n);
    }
    return body;
}

// The slot number in "/photos/<n>" or "/photos/<n>/delete"; -1 if none.
int slotFromUri(const char* uri) {
    int slot = -1;
    return std::sscanf(uri, "/photos/%d", &slot) == 1 ? slot : -1;
}


esp_err_t handlePhotosPage(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    return sendHtml(req, setup_page::renderPhotosPage());
}

esp_err_t handlePhotosList(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    std::vector<int> slots = photo_store::list();
    return sendText(req, "200 OK", "application/json",
                    setup_page::photosListJson(slots, photo_store::kMaxPhotos, photos::kWidth, photos::kHeight));
}

// GET /photos/<n>: the stored 2bpp image, for the page's thumbnails.
esp_err_t handlePhotoGet(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    auto photo = photo_store::open(slotFromUri(req->uri));
    if (!photo) {
        return sendText(req, "404 Not Found", "text/plain", "no such photo");
    }
    httpd_resp_set_type(req, "application/octet-stream");
    return httpd_resp_send(req, reinterpret_cast<const char*>(photo->gray().data()),
                           static_cast<ssize_t>(photo->gray().size()));
}

esp_err_t handlePhotoAdd(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    auto body = readBody<std::vector<uint8_t>>(req, kMaxPhotoBytes);
    if (!body) {
        return ESP_FAIL;
    }
    if (!photo_format::isUpload(*body, photos::kWidth, photos::kHeight)) {
        return sendText(req, "400 Bad Request", "text/plain; charset=utf-8", "照片資料大小不對");
    }
    int slot = photo_store::add(*body, photos::kWidth, photos::kHeight);
    if (slot < 0) {
        return sendText(req, "507 Insufficient Storage", "text/plain; charset=utf-8", "照片已滿或寫入失敗");
    }
    photo_rotation::State rotation = settings::loadPhotoRotation();
    photo_rotation::added(rotation, slot);  // on the panel next
    settings::savePhotoRotation(rotation);
    return sendText(req, "200 OK", "application/json", "{\"slot\":" + std::to_string(slot) + "}");
}

// POST /photos/<n>/delete
esp_err_t handlePhotoDelete(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    int slot = slotFromUri(req->uri);
    if (photo_store::remove(slot) != ESP_OK) {
        return sendText(req, "404 Not Found", "text/plain", "no such photo");
    }
    photo_rotation::State rotation = settings::loadPhotoRotation();
    photo_rotation::removed(rotation, slot);
    settings::savePhotoRotation(rotation);
    return sendText(req, "200 OK", "text/plain", "deleted");
}

// POST /restart, LAN mode only: restarts without touching any settings.
esp_err_t handleRestart(httpd_req_t* req) {
    sendText(req, "200 OK", "text/plain", "restarting");
    xEventGroupSetBits(gEvents, kSavedBit);  // the waiting loop restarts on it
    return ESP_OK;
}

// GET /reset: the confirmation page, the second of two steps.
esp_err_t handleResetConfirm(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    return sendHtml(req, setup_page::renderResetConfirm());
}

// POST /reset (from the confirmation page): erases every setting and photo,
// then restarts into a first setup.
esp_err_t handleReset(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    auto body = readBody<std::string>(req, kMaxFormBytes);
    if (!body) {
        return ESP_FAIL;
    }
    if (!setup_page::confirmsReset(*body)) {
        return sendText(req, "400 Bad Request", "text/plain", "not confirmed");
    }
    for (int slot : photo_store::list()) {
        photo_store::remove(slot);
    }
    settings::eraseAll();
    ESP_LOGW(kTag, "everything erased");
    esp_err_t err = sendHtml(req, setup_page::renderErased());
    xEventGroupSetBits(gEvents, kErasedBit);
    return err;
}

esp_err_t handleSave(httpd_req_t* req) {
    touch();
    if (!authorize(req)) {
        return ESP_OK;
    }
    auto body = readBody<std::string>(req, kMaxFormBytes);
    if (!body) {
        return ESP_FAIL;
    }

    bool wifiStep = gMode == Mode::accessPoint;
    auto render = [&](const Settings& s, std::span<const std::string> errors) {
        return wifiStep ? setup_page::renderWifiForm(s, gNearby, errors)
                        : setup_page::renderForm(s, gNearby, errors);
    };
    setup_page::FormResult result =
        wifiStep ? setup_page::applyWifiForm(gCurrent, *body) : setup_page::applyForm(gCurrent, *body);
    if (!result.errors.empty()) {
        return sendHtml(req, render(result.settings, result.errors));
    }
    if (settings::save(result.settings) != ESP_OK) {
        const std::string errors[] = {"儲存失敗，請再試一次"};
        return sendHtml(req, render(result.settings, errors));
    }
    ESP_LOGI(kTag, "settings saved");
    gCurrent = result.settings;
    esp_err_t err = sendHtml(req, setup_page::renderSaved());
    xEventGroupSetBits(gEvents, kSavedBit);
    return err;
}

// Everything else -- including the URLs phones probe for internet access
// (/generate_204, /hotspot-detect.html, ...) -- redirects to the form, which
// tells the phone there's a captive portal to show.
esp_err_t handleRedirect(httpd_req_t* req) {
    touch();
    ESP_LOGI(kTag, "redirecting %s", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", kPageUrl);
    return httpd_resp_send(req, nullptr, 0);
}

// The handlers depend on the mode: the access point serves only the WiFi
// form, plus a redirect for every other path (which is what makes phones
// pop the page up); the home network serves the rest of the settings and
// the photos (and, in development, /restart).
esp_err_t startHttp() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;  // phones open many probe connections at once
    config.stack_size = 8192;        // the form is built with std::string
    config.max_uri_handlers = 14;
    esp_err_t err = httpd_start(&gServer, &config);
    if (err != ESP_OK) {
        return err;
    }
    static const httpd_uri_t form = {.uri = "/", .method = HTTP_GET, .handler = handleForm, .user_ctx = nullptr};
    static const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = handleSave, .user_ctx = nullptr};
    static const httpd_uri_t reset = {
        .uri = "/reset", .method = HTTP_POST, .handler = handleReset, .user_ctx = nullptr};
    static const httpd_uri_t resetConfirm = {
        .uri = "/reset", .method = HTTP_GET, .handler = handleResetConfirm, .user_ctx = nullptr};
    static const httpd_uri_t photosPage = {
        .uri = "/photos", .method = HTTP_GET, .handler = handlePhotosPage, .user_ctx = nullptr};
    static const httpd_uri_t photosList = {
        .uri = "/photos/list", .method = HTTP_GET, .handler = handlePhotosList, .user_ctx = nullptr};
    static const httpd_uri_t photoAdd = {
        .uri = "/photos/add", .method = HTTP_POST, .handler = handlePhotoAdd, .user_ctx = nullptr};
    static const httpd_uri_t photoGet = {
        .uri = "/photos/*", .method = HTTP_GET, .handler = handlePhotoGet, .user_ctx = nullptr};
    static const httpd_uri_t photoDelete = {
        .uri = "/photos/*", .method = HTTP_POST, .handler = handlePhotoDelete, .user_ctx = nullptr};
    static const httpd_uri_t other = {.uri = "/*", .method = HTTP_GET, .handler = handleRedirect, .user_ctx = nullptr};
    static const httpd_uri_t restart = {
        .uri = "/restart", .method = HTTP_POST, .handler = handleRestart, .user_ctx = nullptr};
    httpd_register_uri_handler(gServer, &form);
    httpd_register_uri_handler(gServer, &save);
    httpd_register_uri_handler(gServer, &reset);
    httpd_register_uri_handler(gServer, &resetConfirm);  // before the access point's catch-all
    if (gMode == Mode::accessPoint) {
        httpd_register_uri_handler(gServer, &other);
        return ESP_OK;
    }
    // First match wins: the specific paths before their wildcards.
    for (const httpd_uri_t* handler : {&photosPage, &photosList, &photoAdd, &photoGet, &photoDelete}) {
        httpd_register_uri_handler(gServer, handler);
    }
    if (gMode == Mode::devLan) {
        httpd_register_uri_handler(gServer, &restart);
    }
    return ESP_OK;
}
}  // namespace

namespace setup_mode {

esp_err_t start(const Settings& current, AccessPoint& out, std::string_view wifiError) {
    gMode = Mode::accessPoint;
    gCurrent = current;
    gRevertedFrom = settings::takeWifiRevertNote();
    if (!wifiError.empty()) {
        gWifiErrors = {std::string(wifiError)};
    }
    gEvents = xEventGroupCreate();
    touch();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* apNetif = esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();  // for the scan
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    gNearby = scanNearby();
    ESP_LOGI(kTag, "%zu networks nearby", gNearby.size());

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ssid[16];
    std::snprintf(ssid, sizeof(ssid), "Glance-%02X%02X", mac[4], mac[5]);
    out.ssid = ssid;
    out.password = setup_page::makeAccessPointPassword([] { return esp_random(); });
    out.url = "http://192.168.4.1";
    out.qrPayload = setup_page::wifiQrPayload(out.ssid, out.password);

    wifi_config_t ap = {};
    std::memcpy(ap.ap.ssid, out.ssid.data(), out.ssid.size());
    ap.ap.ssid_len = static_cast<uint8_t>(out.ssid.size());
    std::memcpy(ap.ap.password, out.password.data(), out.password.size());
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 2;
    ap.ap.channel = 1;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, onStationJoined,
                                                        nullptr, nullptr));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    // RFC 8910: newer phones read the portal's address straight from DHCP.
    esp_netif_dhcps_stop(apNetif);
    esp_netif_dhcps_option(apNetif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, const_cast<char*>(kPageUrl),
                           std::strlen(kPageUrl));
    esp_netif_dhcps_start(apNetif);

    esp_err_t err = startHttp();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "HTTP server failed: %s", esp_err_to_name(err));
        return err;
    }
    xTaskCreate(dnsTask, "captive_dns", 4096, nullptr, 5, nullptr);
    // The password only goes on the screen, never in the log.
    ESP_LOGI(kTag, "access point %s up, %u bytes heap free", out.ssid.c_str(),
             static_cast<unsigned>(esp_get_free_heap_size()));
    return ESP_OK;
}

esp_err_t startHome(const Settings& current, HomeNetwork& out) {
    esp_netif_ip_info_t ip = {};
    esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta || esp_netif_get_ip_info(sta, &ip) != ESP_OK || ip.ip.addr == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    gMode = Mode::home;
    gCurrent = current;
    gRevertedFrom = settings::takeWifiRevertNote();
    gNearby = scanNearby();  // for the WiFi name suggestions
    ESP_LOGI(kTag, "%zu networks nearby", gNearby.size());
    gEvents = xEventGroupCreate();
    touch();
    std::string token = randomHex(16);  // 128 bits
    gAccess.emplace(token);
    char address[16];
    esp_ip4addr_ntoa(&ip.ip, address, sizeof(address));
    out.qrPayload = std::string("http://") + address + "/?t=" + token;
#if CONFIG_GLANCE_DEV_SERIAL_COMMANDS
    // Development builds only, so tests can open it; it's a secret otherwise.
    ESP_LOGW(kTag, "development: setup page at %s", out.qrPayload.c_str());
#endif
    ESP_LOGI(kTag, "home-network setup at http://%s/", address);
    return startHttp();
}

esp_err_t startOnLan(const Settings& current) {
    gMode = Mode::devLan;
    gCurrent = current;  // leaves the revert note for a real setup session
    gEvents = xEventGroupCreate();
    touch();
    return startHttp();
}

bool wasSaved() { return gEvents && (xEventGroupGetBits(gEvents) & (kSavedBit | kErasedBit)); }

Event waitForEvent(uint32_t idleTimeoutMs) {
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(gEvents, kSavedBit | kJoinedBit | kIntruderBit | kErasedBit, pdTRUE,
                                               pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & kIntruderBit) {
            return Event::intruder;
        }
        if (bits & kErasedBit) {
            vTaskDelay(pdMS_TO_TICKS(1500));  // let the page reach the phone
            return Event::erased;
        }
        if (bits & kSavedBit) {
            vTaskDelay(pdMS_TO_TICKS(1500));  // let the "saved" page reach the phone
            return Event::saved;
        }
        if (bits & kJoinedBit) {
            return Event::joined;
        }
        if (esp_timer_get_time() - gLastActivityUs > static_cast<int64_t>(idleTimeoutMs) * 1000) {
            return Event::idle;
        }
    }
}

void close() {
    if (gMode == Mode::accessPoint) {
        esp_wifi_deauth_sta(0);  // everyone
        esp_wifi_stop();
        ESP_LOGW(kTag, "access point closed");
    } else if (gServer) {
        httpd_stop(gServer);
        gServer = nullptr;
        ESP_LOGW(kTag, "setup pages closed");
    }
}

}  // namespace setup_mode
