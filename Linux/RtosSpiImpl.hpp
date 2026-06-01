#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Linux compile-only stubs for RtosSpiBusImpl / RtosSpiDeviceImpl.
// Used for host-side unit tests and utilities. No real SPI hardware access.
// All operations are no-ops that return false.
// ---------------------------------------------------------------------------

class RtosSpiBusImpl
{
public:
    enum class Dma : uint8_t { Disabled, Auto };

    struct Config
    {
        int  host              = 1;
        int  mosi_pin          = -1;
        int  miso_pin          = -1;
        int  sclk_pin          = -1;
        int  max_transfer_bytes = 32;
        Dma  dma               = Dma::Auto;
    };

    RtosSpiBusImpl()                                     = default;
    RtosSpiBusImpl(const RtosSpiBusImpl&)                = delete;
    RtosSpiBusImpl& operator=(const RtosSpiBusImpl&)     = delete;
    RtosSpiBusImpl(RtosSpiBusImpl&&)                     = default;
    RtosSpiBusImpl& operator=(RtosSpiBusImpl&&)          = default;

    bool init(const Config&)  { return false; }
    void deinit()             {}
    bool initialised() const  { return false; }
    void* native()     const  { return nullptr; }
};


class RtosSpiDeviceImpl
{
public:
    struct Config
    {
        int      cs_pin       = -1;
        uint32_t clk_hz       = 1000000;
        uint8_t  mode         = 0;
        uint8_t  command_bits = 0;
        uint8_t  address_bits = 0;
        int      queue_size   = 1;
    };

    RtosSpiDeviceImpl()                                        = default;
    RtosSpiDeviceImpl(const RtosSpiDeviceImpl&)                = delete;
    RtosSpiDeviceImpl& operator=(const RtosSpiDeviceImpl&)     = delete;
    RtosSpiDeviceImpl(RtosSpiDeviceImpl&&)                     = default;
    RtosSpiDeviceImpl& operator=(RtosSpiDeviceImpl&&)          = default;

    bool init(RtosSpiBusImpl&, const Config&)                              { return false; }
    void deinit()                                                          {}
    bool initialised()                                             const   { return false; }
    bool write(const uint8_t*, size_t)                                     { return false; }
    bool read(uint8_t*, size_t)                                            { return false; }
    bool transfer(const uint8_t*, uint8_t*, size_t)                        { return false; }
    bool transfer_cmd(uint16_t, const uint8_t*, uint8_t*, size_t)          { return false; }
    void* native() const                                                   { return nullptr; }
};
