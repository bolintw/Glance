#include "time_sync.hpp"

#include <cstdlib>
#include <ctime>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace {
constexpr const char* kTag = "time_sync";
constexpr const char* kNtpServer = "pool.ntp.org";

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

esp_err_t sync(uint32_t timeoutMs) {
    setenv("TZ", kTaiwanTz, 1);
    tzset();
    // The RTC keeps time through deep sleep; how far NTP has to move it
    // shows how much the RTC clock drifted since the last sync.
    int64_t clockBeforeUs = wallClockUs();
    int64_t monotonicBeforeUs = esp_timer_get_time();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(kNtpServer);
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "sntp init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeoutMs));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "sync timed out/failed: %s", esp_err_to_name(err));
        esp_netif_sntp_deinit();
        return err;
    }

    time_t now = time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
    ESP_LOGI(kTag, "synced: %s (local, %s)", buf, kTaiwanTz);
    if (clockBeforeUs / 1000000 >= kPlausibleEpoch) {
        int64_t expectedUs = clockBeforeUs + (esp_timer_get_time() - monotonicBeforeUs);
        ESP_LOGI(kTag, "RTC clock was off by %+lldms", (wallClockUs() - expectedUs) / 1000);
    }

    esp_netif_sntp_deinit();
    return ESP_OK;
}

}  // namespace time_sync
