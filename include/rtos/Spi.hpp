#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Portable SPI master abstraction.
//
// This header declares the complete interface. Each backend implements the
// methods in backends/<backend>/rtos_spi.cpp; the build system selects which
// backend is compiled. See RTOS_SPI.md for usage examples.
//
// Backend support:
//   - espidf: full implementation (spi_master driver)
//   - zephyr: compile-only stub, all operations return false
//   - linux:  compile-only stub, all operations return false
// ---------------------------------------------------------------------------

namespace rtos
{

// ---------------------------------------------------------------------------
// SpiBus
// One instance per physical SPI host/bus. Non-copyable, movable.
// ---------------------------------------------------------------------------
class SpiBus
{
public:
    enum class Dma : uint8_t
    {
        Disabled,
        Auto,
    };

    struct Config
    {
        int  host               = 1;  ///< Backend host index (ESP-IDF: spi_host_device_t, SPI2_HOST = 1)
        int  mosi_pin           = -1;
        int  miso_pin           = -1;
        int  sclk_pin           = -1;
        int  max_transfer_bytes = 32;
        Dma  dma                = Dma::Auto;
    };

    SpiBus() = default;

    SpiBus(const SpiBus&)            = delete;
    SpiBus& operator=(const SpiBus&) = delete;

    SpiBus(SpiBus&& other) noexcept
        : host_(other.host_), initialised_(other.initialised_)
    {
        other.initialised_ = false;
    }

    SpiBus& operator=(SpiBus&& other) noexcept
    {
        if (this != &other)
        {
            deinit();
            host_              = other.host_;
            initialised_       = other.initialised_;
            other.initialised_ = false;
        }
        return *this;
    }

    ~SpiBus() { deinit(); }

    /// Initialise the SPI master bus. Returns true on success.
    bool init(const Config& cfg);

    /// Free the bus. Safe to call when not initialised.
    void deinit();

    bool initialised() const { return initialised_; }

    /// Backend host index escape hatch (ESP-IDF: cast to spi_host_device_t).
    int native() const { return host_; }

private:
    int  host_        = 1;
    bool initialised_ = false;
};

// ---------------------------------------------------------------------------
// SpiDevice
// One instance per device (chip select) on a bus. Non-copyable, movable.
// ---------------------------------------------------------------------------
class SpiDevice
{
public:
    struct Config
    {
        int      cs_pin       = -1;
        uint32_t clk_hz       = 1000000; ///< SPI clock frequency in Hz
        uint8_t  mode         = 0;       ///< SPI mode 0–3 (encodes CPOL/CPHA)
        uint8_t  command_bits = 0;       ///< Hardware command-phase width (0 = none, 8 = register-address style)
        uint8_t  address_bits = 0;       ///< Hardware address-phase width (0 = none)
        int      queue_size   = 1;       ///< Transaction queue depth (1 = polling, no pipelining)
    };

    SpiDevice() = default;

    SpiDevice(const SpiDevice&)            = delete;
    SpiDevice& operator=(const SpiDevice&) = delete;

    SpiDevice(SpiDevice&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }

    SpiDevice& operator=(SpiDevice&& other) noexcept
    {
        if (this != &other) { deinit(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    ~SpiDevice() { deinit(); }

    /// Register this device on the given bus. Returns true on success.
    bool init(SpiBus& bus, const Config& cfg);

    /// Remove the device from the bus. Safe to call when not initialised.
    void deinit();

    bool initialised() const { return handle_ != nullptr; }

    // -----------------------------------------------------------------------
    // I/O operations — all return true on success.
    // -----------------------------------------------------------------------

    /// Write `len` bytes to the device. No command phase.
    bool write(const uint8_t* data, size_t len);

    /// Read `len` bytes from the device. No command phase.
    bool read(uint8_t* buf, size_t len);

    /// Full-duplex transfer: send `tx` and receive into `rx` simultaneously.
    /// Pass nullptr for tx or rx to send/receive only.
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t len);

    /// Transfer with a hardware command phase.
    /// `cmd` is placed in the command bits configured at init time (e.g. the
    /// 8-bit register address used by devices like IIM42352). Set the MSB for
    /// read (0x80) or leave clear for write, following the device's convention.
    /// Pass nullptr for tx or rx when only one direction is needed.
    bool transfer_cmd(uint16_t cmd, const uint8_t* tx, uint8_t* rx, size_t len);

    /// Backend device handle escape hatch (ESP-IDF: spi_device_handle_t).
    void* native() const { return handle_; }

private:
    void* handle_ = nullptr;
};

} // namespace rtos
