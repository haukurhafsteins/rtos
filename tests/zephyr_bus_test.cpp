#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "rtos/I2C.hpp"
#include "rtos/Spi.hpp"
#include "zephyr_bus_test.hpp"

namespace
{
class ZephyrBusTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        zephyr_bus_test::reset();
    }
};

rtos::SpiBus::Config spiBusConfig()
{
    rtos::SpiBus::Config config;
    config.host = 3;
    config.sclk_pin = 36;
    config.mosi_pin = 37;
    config.miso_pin = 38;
    return config;
}

rtos::I2CBus::Config i2cBusConfig()
{
    rtos::I2CBus::Config config;
    config.port = 1;
    config.sda_pin = 34;
    config.scl_pin = 35;
    config.internal_pullup = false;
    return config;
}
}

TEST_F(ZephyrBusTest, SpiBusMapsSpi3AndRejectsMismatchedPins)
{
    rtos::SpiBus bus;
    EXPECT_TRUE(bus.init(spiBusConfig()));
    EXPECT_TRUE(bus.initialised());
    EXPECT_EQ(bus.native(), 3);

    auto wrongHost = spiBusConfig();
    wrongHost.host = 2;
    rtos::SpiBus invalidHost;
    EXPECT_FALSE(invalidHost.init(wrongHost));

    auto wrongPin = spiBusConfig();
    wrongPin.mosi_pin = 12;
    rtos::SpiBus invalidPin;
    EXPECT_FALSE(invalidPin.init(wrongPin));
}

TEST_F(ZephyrBusTest, SpiTransfersUseConfiguredClockModeAndChipSelect)
{
    rtos::SpiBus bus;
    ASSERT_TRUE(bus.init(spiBusConfig()));
    rtos::SpiDevice::Config config;
    config.cs_pin = 39;
    config.clk_hz = 4000000;
    config.mode = 3;
    config.command_bits = 8;

    rtos::SpiDevice imu;
    ASSERT_TRUE(imu.init(bus, config));
    const std::array<uint8_t, 2> write{0x12, 0x34};
    std::array<uint8_t, 2> read{};
    zephyr_bus_test::setSpiReadData({0x56, 0x78});
    ASSERT_TRUE(imu.transfer(write.data(), read.data(), read.size()));

    EXPECT_EQ(zephyr_bus_test::lastSpiConfig().frequency, 4000000u);
    EXPECT_NE(zephyr_bus_test::lastSpiConfig().operation & SPI_MODE_CPOL, 0);
    EXPECT_NE(zephyr_bus_test::lastSpiConfig().operation & SPI_MODE_CPHA, 0);
    EXPECT_EQ(zephyr_bus_test::lastSpiConfig().cs.gpio.pin, 7u);
    EXPECT_EQ(zephyr_bus_test::lastSpiWrite(),
        (std::vector<uint8_t>{0x12, 0x34}));
    EXPECT_EQ(read, (std::array<uint8_t, 2>{0x56, 0x78}));
}

TEST_F(ZephyrBusTest, SpiCommandTransferPrependsCommandAndDiscardsFirstRxByte)
{
    rtos::SpiBus bus;
    ASSERT_TRUE(bus.init(spiBusConfig()));
    rtos::SpiDevice::Config config;
    config.cs_pin = 39;
    config.command_bits = 8;
    rtos::SpiDevice imu;
    ASSERT_TRUE(imu.init(bus, config));

    std::array<uint8_t, 2> read{};
    zephyr_bus_test::setSpiReadData({0xff, 0xaa, 0xbb});
    ASSERT_TRUE(imu.transfer_cmd(0x80, nullptr, read.data(), read.size()));

    EXPECT_EQ(zephyr_bus_test::lastSpiWrite(),
        (std::vector<uint8_t>{0x80, 0x00, 0x00}));
    EXPECT_EQ(read, (std::array<uint8_t, 2>{0xaa, 0xbb}));
}

TEST_F(ZephyrBusTest, SpiRejectsTransfersBeyondEasyDmaLimit)
{
    rtos::SpiBus bus;
    ASSERT_TRUE(bus.init(spiBusConfig()));
    rtos::SpiDevice::Config config;
    config.cs_pin = 39;
    config.command_bits = 8;
    rtos::SpiDevice imu;
    ASSERT_TRUE(imu.init(bus, config));

    std::vector<uint8_t> oversized(0x10000);
    EXPECT_FALSE(imu.transfer(
        oversized.data(), nullptr, oversized.size()));
    EXPECT_FALSE(imu.transfer_cmd(
        0x80, oversized.data(), nullptr, oversized.size()));
}

TEST_F(ZephyrBusTest, I2cBusMapsI2c1AndRejectsMismatchedPins)
{
    rtos::I2CBus bus;
    EXPECT_TRUE(bus.init(i2cBusConfig()));
    EXPECT_TRUE(bus.initialised());

    auto wrongPort = i2cBusConfig();
    wrongPort.port = 0;
    rtos::I2CBus invalidPort;
    EXPECT_FALSE(invalidPort.init(wrongPort));

    auto wrongPin = i2cBusConfig();
    wrongPin.scl_pin = 12;
    rtos::I2CBus invalidPin;
    EXPECT_FALSE(invalidPin.init(wrongPin));
}

TEST_F(ZephyrBusTest, I2cWriteReadUsesAddressSpeedAndRepeatedStart)
{
    rtos::I2CBus bus;
    ASSERT_TRUE(bus.init(i2cBusConfig()));
    rtos::I2CDevice::Config config;
    config.address = 0x5a;
    config.clk_hz = 400000;
    rtos::I2CDevice haptic;
    ASSERT_TRUE(haptic.init(bus, config));

    const std::array<uint8_t, 1> write{0x0c};
    std::array<uint8_t, 2> read{};
    zephyr_bus_test::setI2cReadData({0x11, 0x22});
    ASSERT_TRUE(haptic.write_read(
        write.data(), write.size(), read.data(), read.size()));

    EXPECT_EQ(zephyr_bus_test::lastI2cAddress(), 0x5a);
    EXPECT_EQ(zephyr_bus_test::lastI2cWrite(),
        (std::vector<uint8_t>{0x0c}));
    ASSERT_EQ(zephyr_bus_test::lastI2cMessages().size(), 2u);
    EXPECT_EQ(zephyr_bus_test::lastI2cMessages()[0].flags, I2C_MSG_WRITE);
    EXPECT_EQ(zephyr_bus_test::lastI2cMessages()[1].flags,
        I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP);
    EXPECT_EQ(zephyr_bus_test::lastI2cConfig(),
        I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_FAST));
    EXPECT_EQ(read, (std::array<uint8_t, 2>{0x11, 0x22}));
}

TEST_F(ZephyrBusTest, I2cTenBitAddressSetsMessageFlag)
{
    rtos::I2CBus bus;
    ASSERT_TRUE(bus.init(i2cBusConfig()));
    rtos::I2CDevice::Config config;
    config.address = 0x2aa;
    config.addr_10bit = true;
    rtos::I2CDevice device;
    ASSERT_TRUE(device.init(bus, config));

    const uint8_t value = 0x44;
    ASSERT_TRUE(device.write(&value, 1));
    ASSERT_EQ(zephyr_bus_test::lastI2cMessages().size(), 1u);
    EXPECT_NE(zephyr_bus_test::lastI2cMessages()[0].flags &
        I2C_MSG_ADDR_10_BITS, 0);
}
