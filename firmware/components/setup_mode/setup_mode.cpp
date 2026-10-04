#include "setup_mode.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "captive_dns.hpp"
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
#include "settings.hpp"
#include "setup_page.hpp"

namespace {
constexpr const char* kTag = "setup_mode";
constexpr std::array<uint8_t, 4> kApIp = {192, 168, 4, 1};  // ESP-IDF's default AP address
constexpr const char* kPageUrl = "http://192.168.4.1/";
constexpr size_t kMaxNearby = 15;
constexpr size_t kMaxFormBytes = 8 * 1024;
constexpr int kSavedBit = BIT0;

// Shared with the HTTP handlers and the DNS task. One setup session per boot.
Settings gCurrent;
std::vector<std::string> gNearby;
EventGroupHandle_t gEvents = nullptr;
std::atomic<int64_t> gLastActivityUs{0};

void touch() { gLastActivityUs = esp_timer_get_time(); }

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

esp_err_t handleForm(httpd_req_t* req) {
    touch();
    return sendHtml(req, setup_page::renderForm(gCurrent, gNearby, {}));
}

esp_err_t handleSave(httpd_req_t* req) {
    touch();
    if (req->content_len > kMaxFormBytes) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "form too large");
        return ESP_FAIL;
    }
    std::string body(req->content_len, '\0');
    size_t received = 0;
    while (received < body.size()) {
        int n = httpd_req_recv(req, body.data() + received, body.size() - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0) {
            return ESP_FAIL;
        }
        received += static_cast<size_t>(n);
    }

    setup_page::FormResult result = setup_page::applyForm(gCurrent, body);
    if (!result.errors.empty()) {
        return sendHtml(req, setup_page::renderForm(result.settings, gNearby, result.errors));
    }
    if (settings::save(result.settings) != ESP_OK) {
        const std::string errors[] = {"儲存失敗，請再試一次"};
        return sendHtml(req, setup_page::renderForm(result.settings, gNearby, errors));
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
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", kPageUrl);
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t startHttp() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;  // phones open many probe connections at once
    config.stack_size = 8192;        // the form is built with std::string
    httpd_handle_t server = nullptr;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        return err;
    }
    static const httpd_uri_t form = {.uri = "/", .method = HTTP_GET, .handler = handleForm, .user_ctx = nullptr};
    static const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = handleSave, .user_ctx = nullptr};
    static const httpd_uri_t other = {.uri = "/*", .method = HTTP_GET, .handler = handleRedirect, .user_ctx = nullptr};
    httpd_register_uri_handler(server, &form);
    httpd_register_uri_handler(server, &save);
    httpd_register_uri_handler(server, &other);
    return ESP_OK;
}
}  // namespace

namespace setup_mode {

esp_err_t start(const Settings& current, AccessPoint& out) {
    gCurrent = current;
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

bool waitForSave(uint32_t idleTimeoutMs) {
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(gEvents, kSavedBit, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & kSavedBit) {
            vTaskDelay(pdMS_TO_TICKS(1500));  // let the "saved" page reach the phone
            return true;
        }
        if (esp_timer_get_time() - gLastActivityUs > static_cast<int64_t>(idleTimeoutMs) * 1000) {
            return false;
        }
    }
}

}  // namespace setup_mode
