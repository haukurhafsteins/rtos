#pragma once

#include <cstdint>

#include <zephyr/device.h>

constexpr uint8_t I2C_MSG_WRITE = 0;
constexpr uint8_t I2C_MSG_READ = 1U << 0;
constexpr uint8_t I2C_MSG_STOP = 1U << 1;
constexpr uint8_t I2C_MSG_RESTART = 1U << 2;
constexpr uint8_t I2C_MSG_ADDR_10_BITS = 1U << 3;
constexpr uint32_t I2C_MODE_CONTROLLER = 1U << 4;
constexpr uint32_t I2C_SPEED_STANDARD = 1;
constexpr uint32_t I2C_SPEED_FAST = 2;
constexpr uint32_t I2C_SPEED_FAST_PLUS = 3;

#define I2C_SPEED_SET(speed) (static_cast<uint32_t>(speed) << 1)

struct i2c_msg
{
    uint8_t* buf = nullptr;
    uint32_t len = 0;
    uint8_t flags = 0;
};

int i2c_configure(const device* controller, uint32_t config);
int i2c_transfer(
    const device* controller, i2c_msg* messages, uint8_t count, uint16_t address);
