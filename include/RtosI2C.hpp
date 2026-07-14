#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Portable I2C master abstraction.
//
// This header declares the complete interface. Each backend implements the
// methods in backends/<backend>/rtos_i2c.cpp; the build system selects which
// backend is compiled. See I2C.md for usage examples.
//
// Backend support:
//   - espidf: full implementation (i2c_master driver, IDF v5+)
//   - zephyr: compile-only stub, all operations return false
//   - linux:  compile-only stub, all operations return false
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// RtosI2CBus
// One instance per physical I2C port. Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosI2CBus
{
public:
    struct Config
    {
        int     port              = 0;    ///< Backend port index (ESP-IDF: I2C_NUM_0 / I2C_NUM_1)
        int     sda_pin           = -1;
        int     scl_pin           = -1;
        bool    internal_pullup   = true;
        uint8_t glitch_filter_cnt = 7;
    };

    RtosI2CBus() = default;

    RtosI2CBus(const RtosI2CBus&)            = delete;
    RtosI2CBus& operator=(const RtosI2CBus&) = delete;

    RtosI2CBus(RtosI2CBus&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }

    RtosI2CBus& operator=(RtosI2CBus&& other) noexcept
    {
        if (this != &other) { deinit(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    ~RtosI2CBus() { deinit(); }

    /// Initialise the I2C master bus. Returns true on success.
    bool init(const Config& cfg);

    /// Free the bus. Safe to call when not initialised.
    void deinit();

    bool initialised() const { return handle_ != nullptr; }

    /// Backend bus handle escape hatch (ESP-IDF: i2c_master_bus_handle_t).
    void* native() const { return handle_; }

private:
    void* handle_ = nullptr;
};

// ---------------------------------------------------------------------------
// RtosI2CDevice
// One instance per device address on a bus. Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosI2CDevice
{
public:
    struct Config
    {
        uint16_t address    = 0;       ///< 7-bit (or 10-bit) device address
        uint32_t clk_hz     = 400000;  ///< SCL frequency in Hz
        bool     addr_10bit = false;   ///< Set true for 10-bit addressing
        int      timeout_ms = 100;     ///< Operation timeout
    };

    RtosI2CDevice() = default;

    RtosI2CDevice(const RtosI2CDevice&)            = delete;
    RtosI2CDevice& operator=(const RtosI2CDevice&) = delete;

    RtosI2CDevice(RtosI2CDevice&& other) noexcept
        : handle_(other.handle_), timeout_ms_(other.timeout_ms_)
    { other.handle_ = nullptr; }

    RtosI2CDevice& operator=(RtosI2CDevice&& other) noexcept
    {
        if (this != &other)
        {
            deinit();
            handle_       = other.handle_;
            timeout_ms_   = other.timeout_ms_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~RtosI2CDevice() { deinit(); }

    /// Register this device on the given bus. Returns true on success.
    bool init(RtosI2CBus& bus, const Config& cfg);

    /// Remove the device from the bus. Safe to call when not initialised.
    void deinit();

    bool initialised() const { return handle_ != nullptr; }

    // -----------------------------------------------------------------------
    // I/O operations — all return true on success.
    // -----------------------------------------------------------------------

    /// Write `len` bytes from `data` to the device.
    bool write(const uint8_t* data, size_t len);

    /// Read `len` bytes from the device into `data`.
    bool read(uint8_t* data, size_t len);

    /// Write `wr_len` bytes then read `rd_len` bytes in a single transaction.
    bool write_read(const uint8_t* wr, size_t wr_len, uint8_t* rd, size_t rd_len);

    /// Backend device handle escape hatch (ESP-IDF: i2c_master_dev_handle_t).
    void* native() const { return handle_; }

private:
    void* handle_     = nullptr;
    int   timeout_ms_ = 100;
};
