#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>

extern "C" {
#include "driver/spi_master.h"
}

// ---------------------------------------------------------------------------
// RtosSpiBusImpl
// Wraps an ESP-IDF spi_host_device_t + spi_bus_initialize / spi_bus_free.
// One bus instance per physical SPI host. Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosSpiBusImpl
{
public:
    enum class Dma : uint8_t
    {
        Disabled = SPI_DMA_DISABLED,
        Auto     = SPI_DMA_CH_AUTO,
    };

    struct Config
    {
        int  host              = SPI2_HOST; ///< spi_host_device_t value (SPI1_HOST=0, SPI2_HOST=1, SPI3_HOST=2)
        int  mosi_pin          = -1;
        int  miso_pin          = -1;
        int  sclk_pin          = -1;
        int  max_transfer_bytes = 32;
        Dma  dma               = Dma::Auto;
    };

    RtosSpiBusImpl() = default;

    RtosSpiBusImpl(const RtosSpiBusImpl&)            = delete;
    RtosSpiBusImpl& operator=(const RtosSpiBusImpl&) = delete;

    RtosSpiBusImpl(RtosSpiBusImpl&& other) noexcept
        : host_(other.host_), initialised_(other.initialised_)
    {
        other.initialised_ = false;
    }

    RtosSpiBusImpl& operator=(RtosSpiBusImpl&& other) noexcept
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

    ~RtosSpiBusImpl() { deinit(); }

    /// Initialise the SPI master bus. Returns true on success.
    bool init(const Config& cfg)
    {
        host_ = static_cast<spi_host_device_t>(cfg.host);

        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num     = cfg.mosi_pin;
        bus_cfg.miso_io_num     = cfg.miso_pin;
        bus_cfg.sclk_io_num     = cfg.sclk_pin;
        bus_cfg.quadwp_io_num   = -1;
        bus_cfg.quadhd_io_num   = -1;
        bus_cfg.max_transfer_sz = cfg.max_transfer_bytes;

        if (spi_bus_initialize(host_, &bus_cfg, static_cast<spi_dma_chan_t>(cfg.dma)) != ESP_OK)
            return false;

        initialised_ = true;
        return true;
    }

    void deinit()
    {
        if (initialised_)
        {
            spi_bus_free(host_);
            initialised_ = false;
        }
    }

    bool initialised() const { return initialised_; }

    /// Raw host escape hatch — lets callers use the native ESP-IDF API directly.
    spi_host_device_t native() const { return host_; }

private:
    spi_host_device_t host_        = SPI2_HOST;
    bool              initialised_ = false;
};


// ---------------------------------------------------------------------------
// RtosSpiDeviceImpl
// Wraps an ESP-IDF spi_device_handle_t for a single device on a bus.
// Non-copyable, movable.
// ---------------------------------------------------------------------------
class RtosSpiDeviceImpl
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

    RtosSpiDeviceImpl() = default;

    RtosSpiDeviceImpl(const RtosSpiDeviceImpl&)            = delete;
    RtosSpiDeviceImpl& operator=(const RtosSpiDeviceImpl&) = delete;

    RtosSpiDeviceImpl(RtosSpiDeviceImpl&& other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }

    RtosSpiDeviceImpl& operator=(RtosSpiDeviceImpl&& other) noexcept
    {
        if (this != &other) { deinit(); handle_ = other.handle_; other.handle_ = nullptr; }
        return *this;
    }

    ~RtosSpiDeviceImpl() { deinit(); }

    /// Register this device on the given bus. Returns true on success.
    bool init(RtosSpiBusImpl& bus, const Config& cfg)
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

        return spi_bus_add_device(bus.native(), &dev_cfg, &handle_) == ESP_OK;
    }

    void deinit()
    {
        if (handle_) { spi_bus_remove_device(handle_); handle_ = nullptr; }
    }

    bool initialised() const { return handle_ != nullptr; }

    // -----------------------------------------------------------------------
    // I/O operations — all return true on success.
    // Use spi_device_polling_transmit (no task-notification overhead).
    // -----------------------------------------------------------------------

    /// Write `len` bytes to the device. No command phase.
    bool write(const uint8_t* data, size_t len)
    {
        spi_transaction_t t = {};
        t.length    = len * 8;
        t.tx_buffer = data;
        return spi_device_polling_transmit(handle_, &t) == ESP_OK;
    }

    /// Read `len` bytes from the device. No command phase.
    bool read(uint8_t* buf, size_t len)
    {
        spi_transaction_t t = {};
        t.length    = len * 8;
        t.rxlength  = len * 8;
        t.rx_buffer = buf;
        return spi_device_polling_transmit(handle_, &t) == ESP_OK;
    }

    /// Full-duplex transfer: send `tx` and receive into `rx` simultaneously.
    /// Pass nullptr for tx or rx to send/receive only.
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t len)
    {
        spi_transaction_t t = {};
        t.length    = len * 8;
        t.tx_buffer = tx;
        t.rx_buffer = rx;
        return spi_device_polling_transmit(handle_, &t) == ESP_OK;
    }

    /// Transfer with a hardware command phase.
    /// `cmd` is placed in the command bits configured at init time (e.g. the
    /// 8-bit register address used by devices like IIM42352). Set the MSB for
    /// read (0x80) or leave clear for write, following the device's convention.
    /// Pass nullptr for tx or rx when only one direction is needed.
    bool transfer_cmd(uint16_t cmd, const uint8_t* tx, uint8_t* rx, size_t len)
    {
        spi_transaction_t t = {};
        t.cmd       = cmd;
        t.length    = len * 8;
        t.tx_buffer = tx;
        t.rx_buffer = rx;
        return spi_device_polling_transmit(handle_, &t) == ESP_OK;
    }

    /// Raw handle escape hatch — use to pass the handle to code not yet
    /// migrated to RtosSpiDevice, or to use advanced ESP-IDF SPI features.
    spi_device_handle_t native() const { return handle_; }

private:
    spi_device_handle_t handle_ = nullptr;
};
