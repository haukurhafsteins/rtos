#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Linux compile-only stubs for RtosI2CBusImpl / RtosI2CDeviceImpl.
// Used for host-side unit tests and utilities. No real I2C hardware access.
// All operations are no-ops that return false.
// ---------------------------------------------------------------------------

class RtosI2CBusImpl
{
public:
    struct Config
    {
        int     port              = 0;
        int     sda_pin           = -1;
        int     scl_pin           = -1;
        bool    internal_pullup   = true;
        uint8_t glitch_filter_cnt = 7;
    };

    RtosI2CBusImpl()                                     = default;
    RtosI2CBusImpl(const RtosI2CBusImpl&)                = delete;
    RtosI2CBusImpl& operator=(const RtosI2CBusImpl&)     = delete;
    RtosI2CBusImpl(RtosI2CBusImpl&&)                     = default;
    RtosI2CBusImpl& operator=(RtosI2CBusImpl&&)          = default;

    bool init(const Config&)  { return false; }
    void deinit()             {}
    bool initialised() const  { return false; }
    void* native()     const  { return nullptr; }
};


class RtosI2CDeviceImpl
{
public:
    struct Config
    {
        uint16_t address    = 0;
        uint32_t clk_hz     = 400000;
        bool     addr_10bit = false;
        int      timeout_ms = 100;
    };

    RtosI2CDeviceImpl()                                        = default;
    RtosI2CDeviceImpl(const RtosI2CDeviceImpl&)                = delete;
    RtosI2CDeviceImpl& operator=(const RtosI2CDeviceImpl&)     = delete;
    RtosI2CDeviceImpl(RtosI2CDeviceImpl&&)                     = default;
    RtosI2CDeviceImpl& operator=(RtosI2CDeviceImpl&&)          = default;

    bool init(RtosI2CBusImpl&, const Config&)                          { return false; }
    void deinit()                                                      {}
    bool initialised()                                         const   { return false; }
    bool write(const uint8_t*, size_t)                                 { return false; }
    bool read(uint8_t*, size_t)                                        { return false; }
    bool write_read(const uint8_t*, size_t, uint8_t*, size_t)         { return false; }
    void* native() const                                               { return nullptr; }
};
