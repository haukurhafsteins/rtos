// Zephyr backend for I2CBus / I2CDevice.

#include "rtos/I2C.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>
#include <zephyr/sys/__assert.h>

using namespace rtos;

#ifndef RTOS_I2C_CONTROLLER_NODE
#define RTOS_I2C_CONTROLLER_NODE DT_NODELABEL(i2c1)
#endif

#ifndef RTOS_I2C_PORT
#define RTOS_I2C_PORT 1
#endif

#define RTOS_I2C_PIN_GROUP \
    DT_CHILD(DT_PINCTRL_BY_NAME(RTOS_I2C_CONTROLLER_NODE, default, 0), group1)

namespace
{
constexpr int pinFromPsel(uint32_t psel)
{
    return static_cast<int>((psel >> NRF_PIN_POS) & NRF_PIN_MSK);
}

constexpr int ExpectedSclPin =
    pinFromPsel(DT_PROP_BY_IDX(RTOS_I2C_PIN_GROUP, psels, 0));
constexpr int ExpectedSdaPin =
    pinFromPsel(DT_PROP_BY_IDX(RTOS_I2C_PIN_GROUP, psels, 1));
constexpr bool ExpectedInternalPullup =
    DT_PROP(RTOS_I2C_PIN_GROUP, bias_pull_up);

const device* const Controller = DEVICE_DT_GET(RTOS_I2C_CONTROLLER_NODE);

struct DeviceHandle
{
    const device* controller = nullptr;
    uint32_t controllerConfig = 0;
    uint16_t address = 0;
    uint8_t addressFlags = 0;
};

DeviceHandle* toHandle(void* handle)
{
    return static_cast<DeviceHandle*>(handle);
}

bool assertValid(bool condition, const char* message)
{
    __ASSERT(condition, "%s", message);
    (void)message;
    return condition;
}

bool pinMatches(int configured, int expected)
{
    return configured == 0 || configured == expected;
}

bool speedConfig(uint32_t frequency, uint32_t& output)
{
    uint32_t speed = 0;
    switch (frequency)
    {
    case 100000:
        speed = I2C_SPEED_STANDARD;
        break;
    case 400000:
        speed = I2C_SPEED_FAST;
        break;
    case 1000000:
        speed = I2C_SPEED_FAST_PLUS;
        break;
    default:
        return false;
    }
    output = I2C_MODE_CONTROLLER | I2C_SPEED_SET(speed);
    return true;
}

bool validBuffer(const void* data, size_t size)
{
    return data != nullptr || size == 0;
}

bool validLength(size_t size)
{
    return size <= std::numeric_limits<uint32_t>::max();
}

bool transfer(DeviceHandle& handle, i2c_msg* messages, uint8_t count)
{
    return i2c_configure(handle.controller, handle.controllerConfig) == 0 &&
        i2c_transfer(handle.controller, messages, count, handle.address) == 0;
}
} // namespace

bool I2CBus::init(const Config& cfg)
{
    deinit();
    if (!assertValid(cfg.port == RTOS_I2C_PORT,
            "rtos I2C port does not match i2c1") ||
        !assertValid(pinMatches(cfg.scl_pin, ExpectedSclPin),
            "rtos I2C SCL pin does not match devicetree") ||
        !assertValid(pinMatches(cfg.sda_pin, ExpectedSdaPin),
            "rtos I2C SDA pin does not match devicetree") ||
        !assertValid(cfg.internal_pullup == ExpectedInternalPullup,
            "rtos I2C pull-up config does not match devicetree") ||
        !device_is_ready(Controller))
        return false;

    // Pin routing, pull resistors, and glitch filtering are devicetree-owned.
    handle_ = const_cast<device*>(Controller);
    return true;
}

void I2CBus::deinit()
{
    handle_ = nullptr;
}

bool I2CDevice::init(I2CBus& bus, const Config& cfg)
{
    deinit();
    const bool addressValid = cfg.addr_10bit
        ? cfg.address <= UINT16_C(0x3ff)
        : cfg.address <= UINT16_C(0x7f);
    uint32_t controllerConfig = 0;
    if (!bus.initialised() || !addressValid || cfg.timeout_ms < 0 ||
        !speedConfig(cfg.clk_hz, controllerConfig))
        return false;

    auto* handle = new (std::nothrow) DeviceHandle{};
    if (handle == nullptr)
        return false;
    handle->controller = static_cast<const device*>(bus.native());
    handle->controllerConfig = controllerConfig;
    handle->address = cfg.address;
    handle->addressFlags = cfg.addr_10bit ? I2C_MSG_ADDR_10_BITS : 0;
    if (i2c_configure(handle->controller, handle->controllerConfig) != 0)
    {
        delete handle;
        return false;
    }

    timeout_ms_ = cfg.timeout_ms;
    handle_ = handle;
    return true;
}

void I2CDevice::deinit()
{
    delete toHandle(handle_);
    handle_ = nullptr;
}

bool I2CDevice::write(const uint8_t* data, size_t len)
{
    auto* handle = toHandle(handle_);
    if (handle == nullptr || !validBuffer(data, len) || !validLength(len))
        return false;
    i2c_msg message{
        const_cast<uint8_t*>(data),
        static_cast<uint32_t>(len),
        static_cast<uint8_t>(handle->addressFlags | I2C_MSG_WRITE | I2C_MSG_STOP),
    };
    return transfer(*handle, &message, 1);
}

bool I2CDevice::read(uint8_t* data, size_t len)
{
    auto* handle = toHandle(handle_);
    if (handle == nullptr || !validBuffer(data, len) || !validLength(len))
        return false;
    i2c_msg message{
        data,
        static_cast<uint32_t>(len),
        static_cast<uint8_t>(handle->addressFlags | I2C_MSG_READ | I2C_MSG_STOP),
    };
    return transfer(*handle, &message, 1);
}

bool I2CDevice::write_read(
    const uint8_t* wr,
    size_t wr_len,
    uint8_t* rd,
    size_t rd_len)
{
    auto* handle = toHandle(handle_);
    if (handle == nullptr || !validBuffer(wr, wr_len) ||
        !validBuffer(rd, rd_len) || !validLength(wr_len) ||
        !validLength(rd_len))
        return false;

    i2c_msg messages[2]{
        {
            const_cast<uint8_t*>(wr),
            static_cast<uint32_t>(wr_len),
            static_cast<uint8_t>(handle->addressFlags | I2C_MSG_WRITE),
        },
        {
            rd,
            static_cast<uint32_t>(rd_len),
            static_cast<uint8_t>(handle->addressFlags | I2C_MSG_RESTART |
                I2C_MSG_READ | I2C_MSG_STOP),
        },
    };
    return transfer(*handle, messages, 2);
}
