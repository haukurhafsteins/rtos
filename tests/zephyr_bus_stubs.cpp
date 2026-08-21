#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>

#include "zephyr_bus_test.hpp"

namespace
{
std::array<device, 16> devices{};
spi_config capturedSpiConfig{};
std::vector<uint8_t> capturedSpiWrite;
std::vector<uint8_t> spiReadData;
uint32_t capturedI2cConfig = 0;
uint16_t capturedI2cAddress = 0;
std::vector<i2c_msg> capturedI2cMessages;
std::vector<uint8_t> capturedI2cWrite;
std::vector<uint8_t> i2cReadData;
}

const device* zephyr_test_device(int id)
{
    auto& value = devices.at(static_cast<std::size_t>(id));
    value.id = id;
    return &value;
}

void zephyr_bus_test::reset()
{
    for (auto& value : devices)
        value.ready = true;
    capturedSpiConfig = {};
    capturedSpiWrite.clear();
    spiReadData.clear();
    capturedI2cConfig = 0;
    capturedI2cAddress = 0;
    capturedI2cMessages.clear();
    capturedI2cWrite.clear();
    i2cReadData.clear();
}

void zephyr_bus_test::setSpiReadData(std::vector<uint8_t> data)
{
    spiReadData = std::move(data);
}

void zephyr_bus_test::setI2cReadData(std::vector<uint8_t> data)
{
    i2cReadData = std::move(data);
}

const spi_config& zephyr_bus_test::lastSpiConfig()
{
    return capturedSpiConfig;
}

const std::vector<uint8_t>& zephyr_bus_test::lastSpiWrite()
{
    return capturedSpiWrite;
}

uint32_t zephyr_bus_test::lastI2cConfig()
{
    return capturedI2cConfig;
}

uint16_t zephyr_bus_test::lastI2cAddress()
{
    return capturedI2cAddress;
}

const std::vector<i2c_msg>& zephyr_bus_test::lastI2cMessages()
{
    return capturedI2cMessages;
}

const std::vector<uint8_t>& zephyr_bus_test::lastI2cWrite()
{
    return capturedI2cWrite;
}

int spi_transceive(
    const device*,
    const spi_config* config,
    const spi_buf_set* tx,
    const spi_buf_set* rx)
{
    capturedSpiConfig = *config;
    capturedSpiWrite.clear();
    if (tx != nullptr)
    {
        for (std::size_t index = 0; index < tx->count; ++index)
        {
            const auto& buffer = tx->buffers[index];
            const auto* bytes = static_cast<const uint8_t*>(buffer.buf);
            for (std::size_t offset = 0; offset < buffer.len; ++offset)
                capturedSpiWrite.push_back(bytes == nullptr ? 0 : bytes[offset]);
        }
    }

    std::size_t readOffset = 0;
    if (rx != nullptr)
    {
        for (std::size_t index = 0; index < rx->count; ++index)
        {
            const auto& buffer = rx->buffers[index];
            auto* bytes = static_cast<uint8_t*>(buffer.buf);
            for (std::size_t offset = 0; offset < buffer.len; ++offset)
            {
                const uint8_t value = readOffset < spiReadData.size()
                    ? spiReadData[readOffset]
                    : 0;
                if (bytes != nullptr)
                    bytes[offset] = value;
                ++readOffset;
            }
        }
    }
    return 0;
}

int i2c_configure(const device*, uint32_t config)
{
    capturedI2cConfig = config;
    return 0;
}

int i2c_transfer(
    const device*, i2c_msg* messages, uint8_t count, uint16_t address)
{
    capturedI2cAddress = address;
    capturedI2cMessages.assign(messages, messages + count);
    capturedI2cWrite.clear();
    std::size_t readOffset = 0;
    for (uint8_t index = 0; index < count; ++index)
    {
        auto& message = messages[index];
        if ((message.flags & I2C_MSG_READ) == 0)
        {
            if (message.len != 0)
            {
                capturedI2cWrite.insert(
                    capturedI2cWrite.end(), message.buf, message.buf + message.len);
            }
            continue;
        }
        for (uint32_t offset = 0; offset < message.len; ++offset)
        {
            message.buf[offset] = readOffset < i2cReadData.size()
                ? i2cReadData[readOffset]
                : 0;
            ++readOffset;
        }
    }
    return 0;
}
