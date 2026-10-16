#include "fuel_gauge.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "max17048.hpp"

namespace {
constexpr const char* kTag = "fuel_gauge";
constexpr gpio_num_t kSdaPin = GPIO_NUM_3;
constexpr gpio_num_t kSclPin = GPIO_NUM_9;  // R26/R27 4.7k pull-ups on the board
constexpr uint32_t kClockHz = 100'000;
constexpr int kTimeoutMs = 50;

// The bus and the gauge on it, released on scope exit.
class Gauge {
public:
    Gauge() {
        i2c_master_bus_config_t bus = {};
        bus.i2c_port = I2C_NUM_0;
        bus.sda_io_num = kSdaPin;
        bus.scl_io_num = kSclPin;
        bus.clk_source = I2C_CLK_SRC_DEFAULT;
        bus.glitch_ignore_cnt = 7;
        if (i2c_new_master_bus(&bus, &bus_) != ESP_OK) {
            bus_ = nullptr;
            return;
        }
        i2c_device_config_t device = {};
        device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device.device_address = max17048::kAddress;
        device.scl_speed_hz = kClockHz;
        if (i2c_master_bus_add_device(bus_, &device, &device_) != ESP_OK) {
            device_ = nullptr;
        }
    }
    ~Gauge() {
        if (device_) {
            i2c_master_bus_rm_device(device_);
        }
        if (bus_) {
            i2c_del_master_bus(bus_);
        }
    }
    Gauge(const Gauge&) = delete;
    Gauge& operator=(const Gauge&) = delete;

    std::optional<uint16_t> read(uint8_t reg) {
        uint8_t data[2];
        if (!device_ || i2c_master_transmit_receive(device_, &reg, 1, data, sizeof(data), kTimeoutMs) != ESP_OK) {
            return std::nullopt;
        }
        return static_cast<uint16_t>(data[0] << 8 | data[1]);
    }

    bool write(uint8_t reg, uint16_t value) {
        uint8_t data[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
        return device_ && i2c_master_transmit(device_, data, sizeof(data), kTimeoutMs) == ESP_OK;
    }

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t device_ = nullptr;
};
}  // namespace

namespace fuel_gauge {

std::optional<Reading> read() {
    Gauge gauge;
    auto version = gauge.read(max17048::kVersion);
    if (!version || !max17048::isVersion(*version)) {
        ESP_LOGW(kTag, "no fuel gauge answering (no battery?)");
        return std::nullopt;
    }
    auto vcell = gauge.read(max17048::kVcell);
    auto soc = gauge.read(max17048::kSoc);
    auto crate = gauge.read(max17048::kCrate);
    auto config = gauge.read(max17048::kConfig);
    auto status = gauge.read(max17048::kStatus);
    if (!vcell || !soc || !crate || !config || !status) {
        ESP_LOGW(kTag, "fuel gauge read failed");
        return std::nullopt;
    }
    bool alert = *config & max17048::kConfigAlert;
    if (*status & max17048::kStatusAlerts) {
        gauge.write(max17048::kStatus, *status & ~max17048::kStatusAlerts);
    }
    if (alert) {
        gauge.write(max17048::kConfig, *config & ~max17048::kConfigAlert);
        ESP_LOGW(kTag, "alert cleared (status 0x%04x)", *status);
    }
    if (gauge.read(max17048::kHibrt) != max17048::kHibrtAlways) {
        bool set = gauge.write(max17048::kHibrt, max17048::kHibrtAlways);
        ESP_LOGI(kTag, "hibernate always: %s", set ? "set" : "write failed");
    }
    return Reading{max17048::millivolts(*vcell), *soc, static_cast<int16_t>(*crate), alert};
}

}  // namespace fuel_gauge
