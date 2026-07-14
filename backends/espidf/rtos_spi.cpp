// ESP-IDF backend for SpiBus / SpiDevice (spi_master driver).

#include "rtos/Spi.hpp"

extern "C" {
#include "driver/spi_master.h"

using namespace rtos;
}

namespace
{
spi_dma_chan_t to_native(SpiBus::Dma dma)
{
    return dma == SpiBus::Dma::Disabled ? SPI_DMA_DISABLED : SPI_DMA_CH_AUTO;
}

spi_device_handle_t dev(void* handle)
{
    return static_cast<spi_device_handle_t>(handle);
}
} // namespace

// ---------------------------------------------------------------------------
// SpiBus
// ---------------------------------------------------------------------------

bool SpiBus::init(const Config& cfg)
{
    host_ = cfg.host;

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = cfg.mosi_pin;
    bus_cfg.miso_io_num     = cfg.miso_pin;
    bus_cfg.sclk_io_num     = cfg.sclk_pin;
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    bus_cfg.max_transfer_sz = cfg.max_transfer_bytes;

    if (spi_bus_initialize(static_cast<spi_host_device_t>(host_), &bus_cfg, to_native(cfg.dma)) != ESP_OK)
        return false;

    initialised_ = true;
    return true;
}

void SpiBus::deinit()
{
    if (initialised_)
    {
        spi_bus_free(static_cast<spi_host_device_t>(host_));
        initialised_ = false;
    }
}

// ---------------------------------------------------------------------------
// SpiDevice
// All transfers use spi_device_polling_transmit (no task-notification
// overhead).
// ---------------------------------------------------------------------------

bool SpiDevice::init(SpiBus& bus, const Config& cfg)
{
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.command_bits   = cfg.command_bits;
    dev_cfg.address_bits   = cfg.address_bits;
    dev_cfg.dummy_bits     = 0;
    dev_cfg.mode           = cfg.mode;
    dev_cfg.clock_source   = SPI_CLK_SRC_DEFAULT;
    dev_cfg.clock_speed_hz = static_cast<int>(cfg.clk_hz);
    dev_cfg.spics_io_num   = cfg.cs_pin;
    dev_cfg.queue_size     = cfg.queue_size;

    spi_device_handle_t handle = nullptr;
    if (spi_bus_add_device(static_cast<spi_host_device_t>(bus.native()), &dev_cfg, &handle) != ESP_OK)
        return false;

    handle_ = handle;
    return true;
}

void SpiDevice::deinit()
{
    if (handle_)
    {
        spi_bus_remove_device(dev(handle_));
        handle_ = nullptr;
    }
}

bool SpiDevice::write(const uint8_t* data, size_t len)
{
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.tx_buffer = data;
    return spi_device_polling_transmit(dev(handle_), &t) == ESP_OK;
}

bool SpiDevice::read(uint8_t* buf, size_t len)
{
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.rxlength  = len * 8;
    t.rx_buffer = buf;
    return spi_device_polling_transmit(dev(handle_), &t) == ESP_OK;
}

bool SpiDevice::transfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    spi_transaction_t t = {};
    t.length    = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_polling_transmit(dev(handle_), &t) == ESP_OK;
}

bool SpiDevice::transfer_cmd(uint16_t cmd, const uint8_t* tx, uint8_t* rx, size_t len)
{
    spi_transaction_t t = {};
    t.cmd       = cmd;
    t.length    = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_polling_transmit(dev(handle_), &t) == ESP_OK;
}
