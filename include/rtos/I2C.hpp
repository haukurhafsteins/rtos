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

namespace rtos
{

// ---------------------------------------------------------------------------
// I2CBus
// One instance per physical I2C port. Non-copyable, movable.
// ---------------------------------------------------------------------------
class I2CBus
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

    I2CBus() = default;

    I2CBus(const I2CBus&)            = delete;
    I2CBus& operator=(const I2CBus&) = delete;

    I2CBus(I2CBus&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }

    I2CBus& operator=(I2CBus&& other) noexcept
    {
        if (this != &other) { deinit(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    ~I2CBus() { deinit(); }

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
// I2CDevice
// One instance per device address on a bus. Non-copyable, movable.
// ---------------------------------------------------------------------------
class I2CDevice
{
public:
    struct Config
    {
        uint16_t address    = 0;       ///< 7-bit (or 10-bit) device address
        uint32_t clk_hz     = 400000;  ///< SCL frequency in Hz
        bool     addr_10bit = false;   ///< Set true for 10-bit addressing
        int      timeout_ms = 100;     ///< Operation timeout
    };

    I2CDevice() = default;

    I2CDevice(const I2CDevice&)            = delete;
    I2CDevice& operator=(const I2CDevice&) = delete;

    I2CDevice(I2CDevice&& other) noexcept
        : handle_(other.handle_), timeout_ms_(other.timeout_ms_)
    { other.handle_ = nullptr; }

    I2CDevice& operator=(I2CDevice&& other) noexcept
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

    ~I2CDevice() { deinit(); }

    /// Register this device on the given bus. Returns true on success.
    bool init(I2CBus& bus, const Config& cfg);

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

} // namespace rtos
