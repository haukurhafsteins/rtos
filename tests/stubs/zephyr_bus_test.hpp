#pragma once

#include <cstdint>
#include <vector>

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>

namespace zephyr_bus_test
{
void reset();
void setSpiReadData(std::vector<uint8_t> data);
void setI2cReadData(std::vector<uint8_t> data);
const spi_config& lastSpiConfig();
const std::vector<uint8_t>& lastSpiWrite();
uint32_t lastI2cConfig();
uint16_t lastI2cAddress();
const std::vector<i2c_msg>& lastI2cMessages();
const std::vector<uint8_t>& lastI2cWrite();
}
