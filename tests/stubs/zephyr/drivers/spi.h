#pragma once

#include <cstddef>
#include <cstdint>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

using spi_operation_t = uint16_t;

constexpr spi_operation_t SPI_OP_MODE_MASTER = 0;
constexpr spi_operation_t SPI_MODE_CPOL = 1U << 1;
constexpr spi_operation_t SPI_MODE_CPHA = 1U << 2;
constexpr spi_operation_t SPI_TRANSFER_MSB = 0;

#define SPI_WORD_SET(bits) (static_cast<spi_operation_t>(bits) << 5)

struct spi_cs_control
{
    gpio_dt_spec gpio{};
    uint32_t delay = 0;
};

struct spi_config
{
    uint32_t frequency = 0;
    spi_operation_t operation = 0;
    uint16_t slave = 0;
    spi_cs_control cs{};
};

struct spi_buf
{
    void* buf = nullptr;
    std::size_t len = 0;
};

struct spi_buf_set
{
    const spi_buf* buffers = nullptr;
    std::size_t count = 0;
};

int spi_transceive(
    const device* controller,
    const spi_config* config,
    const spi_buf_set* tx,
    const spi_buf_set* rx);
