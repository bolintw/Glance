#include "time_sync.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <ctime>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace {
constexpr const char* kTag = "time_sync";
// Asked in turn, a few seconds each, until one answers: SNTP is a single
// UDP packet, and lwIP only sends it again after 15 s -- past our timeout --
// so a lost packet, or a pool.ntp.org volunteer that doesn't answer, used to
// fail the whole run (once right after an update, which then rolled back).
constexpr const char* kNtpServers[] = {"time.cloudflare.com", "time.google.com", "pool.ntp.org"};
constexpr uint32_t kAttemptMs = 3000;

// POSIX TZ strings encode the offset as "hours to ADD to local time to get
// UTC", which is backwards from how people normally say it -- "CST-8" is the
// correct way to express UTC+8 (Taiwan has no DST, so no second rule needed).
constexpr const char* kTaiwanTz = "CST-8";

// Anything before this is the RTC's power-on default, not a kept clock.
constexpr time_t kPlausibleEpoch = 1'700'000'000;  // 2023-11

int64_t wallClockUs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}
}  // namespace

namespace time_sync {

bool clockIsPlausible() { return time(nullptr) >= kPlausibleEpoch; }

void useLocalTimezone() {
    setenv("TZ", kTaiwanTz, 1);
    tzset();
}

esp_err_t sync(uint32_t timeoutMs) {
    useLocalTimezone();
    // The RTC keeps time through deep sleep; how far NTP has to move it
    // shows how much the RTC clock drifted since the last sync.
    int64_t clockBeforeUs = wallClockUs();
    int64_t monotonicBeforeUs = esp_timer_get_time();

    const int64_t deadlineUs = monotonicBeforeUs + static_cast<int64_t>(timeoutMs) * 1000;
    esp_err_t err = ESP_ERR_TIMEOUT;
    const char* server = nullptr;
    for (int attempt = 0; err != ESP_OK; attempt++) {
        const int64_t leftMs = (deadlineUs - esp_timer_get_time()) / 1000;
        if (leftMs <= 0) {
            ESP_LOGE(kTag, "sync timed out after %d tries", attempt);
            return ESP_ERR_TIMEOUT;
        }
        server = kNtpServers[attempt % std::size(kNtpServers)];
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(server);
        err = esp_netif_sntp_init(&config);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "sntp init failed: %s", esp_err_to_name(err));
            return err;
        }
        const uint32_t waitMs = std::min<uint32_t>(kAttemptMs, static_cast<uint32_t>(leftMs));
        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(waitMs));
        esp_netif_sntp_deinit();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "%s didn't answer in %lu ms", server, static_cast<unsigned long>(waitMs));
        }
    }

    time_t now = time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
    ESP_LOGI(kTag, "synced: %s (local, %s) from %s", buf, kTaiwanTz, server);
    if (clockBeforeUs / 1000000 >= kPlausibleEpoch) {
        int64_t expectedUs = clockBeforeUs + (esp_timer_get_time() - monotonicBeforeUs);
        ESP_LOGI(kTag, "RTC clock was off by %+lldms", (wallClockUs() - expectedUs) / 1000);
    }

    return ESP_OK;
}

}  // namespace time_sync
