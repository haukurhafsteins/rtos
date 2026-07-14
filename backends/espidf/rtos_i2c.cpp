// ESP-IDF backend for RtosI2CBus / RtosI2CDevice (i2c_master driver, IDF v5+).

#include "RtosI2C.hpp"

extern "C" {
#include "driver/i2c_master.h"
}

namespace
{
i2c_master_bus_handle_t bus_handle(void* handle)
{
    return static_cast<i2c_master_bus_handle_t>(handle);
}

i2c_master_dev_handle_t dev_handle(void* handle)
{
    return static_cast<i2c_master_dev_handle_t>(handle);
}
} // namespace

// ---------------------------------------------------------------------------
// RtosI2CBus
// ---------------------------------------------------------------------------

bool RtosI2CBus::init(const Config& cfg)
{
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port                     = static_cast<i2c_port_num_t>(cfg.port);
    bus_cfg.sda_io_num                   = static_cast<gpio_num_t>(cfg.sda_pin);
    bus_cfg.scl_io_num                   = static_cast<gpio_num_t>(cfg.scl_pin);
    bus_cfg.clk_source                   = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt            = cfg.glitch_filter_cnt;
    bus_cfg.intr_priority                = 0;
    bus_cfg.trans_queue_depth            = 0;
    bus_cfg.flags.enable_internal_pullup = cfg.internal_pullup ? 1u : 0u;

    i2c_master_bus_handle_t handle = nullptr;
    if (i2c_new_master_bus(&bus_cfg, &handle) != ESP_OK)
        return false;

    handle_ = handle;
    return true;
}

void RtosI2CBus::deinit()
{
    if (handle_)
    {
        i2c_del_master_bus(bus_handle(handle_));
        handle_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// RtosI2CDevice
// ---------------------------------------------------------------------------

bool RtosI2CDevice::init(RtosI2CBus& bus, const Config& cfg)
{
    timeout_ms_ = cfg.timeout_ms;

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = cfg.addr_10bit ? I2C_ADDR_BIT_LEN_10 : I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = cfg.address;
    dev_cfg.scl_speed_hz    = cfg.clk_hz;

    i2c_master_dev_handle_t handle = nullptr;
    if (i2c_master_bus_add_device(bus_handle(bus.native()), &dev_cfg, &handle) != ESP_OK)
        return false;

    handle_ = handle;
    return true;
}

void RtosI2CDevice::deinit()
{
    if (handle_)
    {
        i2c_master_bus_rm_device(dev_handle(handle_));
        handle_ = nullptr;
    }
}

bool RtosI2CDevice::write(const uint8_t* data, size_t len)
{
    return i2c_master_transmit(dev_handle(handle_), data, len, timeout_ms_) == ESP_OK;
}

bool RtosI2CDevice::read(uint8_t* data, size_t len)
{
    return i2c_master_receive(dev_handle(handle_), data, len, timeout_ms_) == ESP_OK;
}

bool RtosI2CDevice::write_read(const uint8_t* wr, size_t wr_len, uint8_t* rd, size_t rd_len)
{
    return i2c_master_transmit_receive(dev_handle(handle_), wr, wr_len, rd, rd_len, timeout_ms_) == ESP_OK;
}
