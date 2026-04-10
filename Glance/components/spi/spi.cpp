#include "spi.hpp"
#include "driver/spi_master.h"

Spi::Spi(const SpiConfig& config) {
    esp_err_t ret;
    spi_host_device_t host_device = static_cast<spi_host_device_t>(config.host_id);
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = config.miso_pin;
    buscfg.mosi_io_num = config.mosi_pin;
    buscfg.sclk_io_num = config.sclk_pin;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = config.max_transfer_size;

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = config.clock_speed_hz;
    devcfg.mode = 0;
    devcfg.spics_io_num = config.cs_pin;
    devcfg.queue_size = 7;
    devcfg.flags = SPI_DEVICE_NO_DUMMY;

    //Initialize the SPI bus
    ret = spi_bus_initialize(host_device, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret = spi_bus_add_device(host_device, &devcfg, (reinterpret_cast<spi_device_handle_t*>(&this->handle_t)));
    ESP_ERROR_CHECK(ret);
}

bool Spi::write(std::span<const uint8_t> data) {
    if (data.size() == 0) {
        return false;    //no need to send anything
    }

    esp_err_t ret;
    spi_transaction_t t = {};
    t.length = data.size() * 8; // bits
    t.tx_buffer = data.data();
    ret = spi_device_polling_transmit(static_cast<spi_device_handle_t>(this->handle_t), &t); //Transmit!
    assert(ret == ESP_OK);          //Should have had no issues.
    return true;
}
