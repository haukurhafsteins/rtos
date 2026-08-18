// Zephyr backend for SpiBus / SpiDevice.

#include "rtos/Spi.hpp"

#include <cstddef>
#include <cstdint>
#include <new>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>
#include <zephyr/sys/__assert.h>

using namespace rtos;

#ifndef RTOS_SPI_CONTROLLER_NODE
#define RTOS_SPI_CONTROLLER_NODE DT_NODELABEL(spi3)
#endif

#ifndef RTOS_SPI_HOST
#define RTOS_SPI_HOST 3
#endif

#define RTOS_SPI_PIN_GROUP \
    DT_CHILD(DT_PINCTRL_BY_NAME(RTOS_SPI_CONTROLLER_NODE, default, 0), group1)

namespace
{
// nRF SPIM EasyDMA MAXCNT is 16 bits wide.
constexpr size_t MaxEasyDmaTransferBytes = UINT16_MAX;

constexpr int pinFromPsel(uint32_t psel)
{
    return static_cast<int>((psel >> NRF_PIN_POS) & NRF_PIN_MSK);
}

constexpr int ExpectedSclkPin =
    pinFromPsel(DT_PROP_BY_IDX(RTOS_SPI_PIN_GROUP, psels, 0));
constexpr int ExpectedMosiPin =
    pinFromPsel(DT_PROP_BY_IDX(RTOS_SPI_PIN_GROUP, psels, 1));
constexpr int ExpectedMisoPin =
    pinFromPsel(DT_PROP_BY_IDX(RTOS_SPI_PIN_GROUP, psels, 2));
constexpr int ExpectedCsPin =
    (DT_PROP_BY_PHANDLE_IDX(
         RTOS_SPI_CONTROLLER_NODE, cs_gpios, 0, port) << 5) |
    DT_GPIO_PIN_BY_IDX(RTOS_SPI_CONTROLLER_NODE, cs_gpios, 0);

const device* const Controller = DEVICE_DT_GET(RTOS_SPI_CONTROLLER_NODE);
const gpio_dt_spec ChipSelect =
    GPIO_DT_SPEC_GET_BY_IDX(RTOS_SPI_CONTROLLER_NODE, cs_gpios, 0);

struct DeviceHandle
{
    const device* controller = nullptr;
    spi_config config{};
    uint8_t commandBits = 0;
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

spi_operation_t operationForMode(uint8_t mode)
{
    spi_operation_t operation =
        SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8);
    if ((mode & 1U) != 0)
        operation |= SPI_MODE_CPHA;
    if ((mode & 2U) != 0)
        operation |= SPI_MODE_CPOL;
    return operation;
}
} // namespace

bool SpiBus::init(const Config& cfg)
{
    if (!assertValid(cfg.host == RTOS_SPI_HOST,
            "rtos SPI host does not match spi3") ||
        !assertValid(pinMatches(cfg.sclk_pin, ExpectedSclkPin),
            "rtos SPI SCLK pin does not match devicetree") ||
        !assertValid(pinMatches(cfg.mosi_pin, ExpectedMosiPin),
            "rtos SPI MOSI pin does not match devicetree") ||
        !assertValid(pinMatches(cfg.miso_pin, ExpectedMisoPin),
            "rtos SPI MISO pin does not match devicetree") ||
        cfg.max_transfer_bytes <= 0 || !device_is_ready(Controller))
        return false;

    host_ = cfg.host;
    initialised_ = true;
    return true;
}

void SpiBus::deinit()
{
    initialised_ = false;
}

bool SpiDevice::init(SpiBus& bus, const Config& cfg)
{
    deinit();
    if (!bus.initialised() ||
        !assertValid(pinMatches(cfg.cs_pin, ExpectedCsPin),
            "rtos SPI chip-select pin does not match devicetree") ||
        !assertValid(cfg.mode <= 3, "rtos SPI mode is invalid") ||
        !assertValid(cfg.command_bits == 0 || cfg.command_bits == 8,
            "Zephyr rtos SPI supports zero or eight command bits") ||
        !assertValid(cfg.address_bits == 0,
            "Zephyr rtos SPI does not support a separate address phase") ||
        cfg.clk_hz == 0 || cfg.queue_size <= 0 ||
        !device_is_ready(Controller) || !gpio_is_ready_dt(&ChipSelect))
        return false;

    auto* handle = new (std::nothrow) DeviceHandle{};
    if (handle == nullptr)
        return false;

    handle->controller = Controller;
    handle->config.frequency = cfg.clk_hz;
    handle->config.operation = operationForMode(cfg.mode);
    handle->config.slave = 0;
    handle->config.cs.gpio = ChipSelect;
    handle->config.cs.delay = 0;
    handle->commandBits = cfg.command_bits;
    handle_ = handle;
    return true;
}

void SpiDevice::deinit()
{
    delete toHandle(handle_);
    handle_ = nullptr;
}

bool SpiDevice::write(const uint8_t* data, size_t len)
{
    return transfer(data, nullptr, len);
}

bool SpiDevice::read(uint8_t* buf, size_t len)
{
    return transfer(nullptr, buf, len);
}

bool SpiDevice::transfer(const uint8_t* tx, uint8_t* rx, size_t len)
{
    auto* handle = toHandle(handle_);
    if (handle == nullptr || len > MaxEasyDmaTransferBytes ||
        (len != 0 && tx == nullptr && rx == nullptr))
        return false;
    if (len == 0)
        return true;

    spi_buf txBuffer{const_cast<uint8_t*>(tx), len};
    spi_buf rxBuffer{rx, len};
    const spi_buf_set txSet{&txBuffer, 1};
    const spi_buf_set rxSet{&rxBuffer, 1};
    return spi_transceive(
        handle->controller,
        &handle->config,
        tx == nullptr ? nullptr : &txSet,
        rx == nullptr ? nullptr : &rxSet) == 0;
}

bool SpiDevice::transfer_cmd(
    uint16_t cmd,
    const uint8_t* tx,
    uint8_t* rx,
    size_t len)
{
    auto* handle = toHandle(handle_);
    if (handle == nullptr || handle->commandBits != 8 || cmd > UINT8_MAX ||
        len > MaxEasyDmaTransferBytes ||
        (len != 0 && tx == nullptr && rx == nullptr))
        return false;

    uint8_t command = static_cast<uint8_t>(cmd);
    spi_buf txBuffers[2]{
        {&command, sizeof(command)},
        {const_cast<uint8_t*>(tx), len},
    };
    spi_buf rxBuffers[2]{
        {nullptr, sizeof(command)},
        {rx, len},
    };
    const spi_buf_set txSet{txBuffers, len == 0 ? 1U : 2U};
    const spi_buf_set rxSet{rxBuffers, len == 0 ? 1U : 2U};
    return spi_transceive(
        handle->controller,
        &handle->config,
        &txSet,
        rx == nullptr ? nullptr : &rxSet) == 0;
}
