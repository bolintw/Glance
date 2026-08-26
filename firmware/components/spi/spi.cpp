#include "spi.hpp"

#include "driver/spi_master.h"
#include "esp_check.h"

Spi::Spi(const SpiConfig& config) {
    auto host = static_cast<spi_host_device_t>(config.hostId);

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = config.mosiPin;
    busConfig.miso_io_num = config.misoPin;
    busConfig.sclk_io_num = config.sclkPin;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = config.maxTransferSize;
    ESP_ERROR_CHECK(spi_bus_initialize(host, &busConfig, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.clock_speed_hz = config.clockSpeedHz;
    deviceConfig.mode = 0;
    deviceConfig.spics_io_num = config.csPin;
    deviceConfig.queue_size = 7;
    deviceConfig.flags = SPI_DEVICE_NO_DUMMY;
    ESP_ERROR_CHECK(
        spi_bus_add_device(host, &deviceConfig, reinterpret_cast<spi_device_handle_t*>(&deviceHandle_)));
}

void Spi::write(std::span<const uint8_t> data) {
    if (data.empty()) {
        return;
    }
    spi_transaction_t transaction = {};
    transaction.length = data.size() * 8;
    transaction.tx_buffer = data.data();
    ESP_ERROR_CHECK(
        spi_device_polling_transmit(static_cast<spi_device_handle_t>(deviceHandle_), &transaction));
}
