#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>

extern "C" {
#include "driver/i2c_master.h"
}

// ---------------------------------------------------------------------------
// RtosI2CBusImpl
// Wraps an ESP-IDF i2c_master_bus_handle_t (new i2c_master_* API, IDF v5+).
// One bus instance per physical I2C port. Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosI2CBusImpl
{
public:
    struct Config
    {
        int           port              = 0;    ///< I2C_NUM_0 / I2C_NUM_1
        int           sda_pin           = -1;
        int           scl_pin           = -1;
        bool          internal_pullup   = true;
        uint8_t       glitch_filter_cnt = 7;
    };

    RtosI2CBusImpl() = default;

    RtosI2CBusImpl(const RtosI2CBusImpl&)            = delete;
    RtosI2CBusImpl& operator=(const RtosI2CBusImpl&) = delete;

    RtosI2CBusImpl(RtosI2CBusImpl&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }

    RtosI2CBusImpl& operator=(RtosI2CBusImpl&& other) noexcept
    {
        if (this != &other) { deinit(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    ~RtosI2CBusImpl() { deinit(); }

    /// Initialise the I2C master bus. Returns true on success.
    bool init(const Config& cfg)
    {
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.i2c_port              = static_cast<i2c_port_num_t>(cfg.port);
        bus_cfg.sda_io_num            = static_cast<gpio_num_t>(cfg.sda_pin);
        bus_cfg.scl_io_num            = static_cast<gpio_num_t>(cfg.scl_pin);
        bus_cfg.clk_source            = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt     = cfg.glitch_filter_cnt;
        bus_cfg.intr_priority         = 0;
        bus_cfg.trans_queue_depth     = 0;
        bus_cfg.flags.enable_internal_pullup = cfg.internal_pullup ? 1u : 0u;
        return i2c_new_master_bus(&bus_cfg, &handle_) == ESP_OK;
    }

    void deinit()
    {
        if (handle_) { i2c_del_master_bus(handle_); handle_ = nullptr; }
    }

    bool initialised() const { return handle_ != nullptr; }

    /// Raw handle escape hatch — lets existing code coexist without migrating.
    i2c_master_bus_handle_t native() const { return handle_; }

private:
    i2c_master_bus_handle_t handle_ = nullptr;
};


// ---------------------------------------------------------------------------
// RtosI2CDeviceImpl
// Wraps an ESP-IDF i2c_master_dev_handle_t for a single device on a bus.
// Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosI2CDeviceImpl
{
public:
    struct Config
    {
        uint16_t address    = 0;       ///< 7-bit (or 10-bit) device address
        uint32_t clk_hz     = 400000;  ///< SCL frequency in Hz
        bool     addr_10bit = false;   ///< Set true for 10-bit addressing
        int      timeout_ms = 100;     ///< Operation timeout
    };

    RtosI2CDeviceImpl() = default;

    RtosI2CDeviceImpl(const RtosI2CDeviceImpl&)            = delete;
    RtosI2CDeviceImpl& operator=(const RtosI2CDeviceImpl&) = delete;

    RtosI2CDeviceImpl(RtosI2CDeviceImpl&& other) noexcept
        : handle_(other.handle_), timeout_ms_(other.timeout_ms_)
    { other.handle_ = nullptr; }

    RtosI2CDeviceImpl& operator=(RtosI2CDeviceImpl&& other) noexcept
    {
        if (this != &other)
        {
            deinit();
            handle_     = other.handle_;
            timeout_ms_ = other.timeout_ms_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~RtosI2CDeviceImpl() { deinit(); }

    /// Register this device on the given bus. Returns true on success.
    bool init(RtosI2CBusImpl& bus, const Config& cfg)
    {
        timeout_ms_ = cfg.timeout_ms;
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = cfg.addr_10bit ? I2C_ADDR_BIT_LEN_10 : I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address  = cfg.address;
        dev_cfg.scl_speed_hz    = cfg.clk_hz;
        return i2c_master_bus_add_device(bus.native(), &dev_cfg, &handle_) == ESP_OK;
    }

    void deinit()
    {
        if (handle_) { i2c_master_bus_rm_device(handle_); handle_ = nullptr; }
    }

    bool initialised() const { return handle_ != nullptr; }

    // -----------------------------------------------------------------------
    // I/O operations — all return true on success.
    // -----------------------------------------------------------------------

    /// Write `len` bytes from `data` to the device.
    bool write(const uint8_t* data, size_t len)
    {
        return i2c_master_transmit(handle_, data, len, timeout_ms_) == ESP_OK;
    }

    /// Read `len` bytes from the device into `data`.
    bool read(uint8_t* data, size_t len)
    {
        return i2c_master_receive(handle_, data, len, timeout_ms_) == ESP_OK;
    }

    /// Write `wr_len` bytes then read `rd_len` bytes in a single transaction.
    bool write_read(const uint8_t* wr, size_t wr_len, uint8_t* rd, size_t rd_len)
    {
        return i2c_master_transmit_receive(handle_, wr, wr_len, rd, rd_len, timeout_ms_) == ESP_OK;
    }

    /// Raw handle escape hatch.
    i2c_master_dev_handle_t native() const { return handle_; }

private:
    i2c_master_dev_handle_t handle_     = nullptr;
    int                     timeout_ms_ = 100;
};
