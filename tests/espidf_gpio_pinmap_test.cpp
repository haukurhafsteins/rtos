#include <gtest/gtest.h>

#include "rtos_gpio_pinmap.hpp"

// The ESP-IDF backend's pin-id resolution, compiled on the host against the
// ESP32-S3 validity mask (tests/stubs_gpio_espidf/driver/gpio.h). What this cannot
// cover is the driver itself: that Pin::make(46, ...) really drives GPIO 46 is
// checked on a meter (HMO-74).
using rtos::gpio::espidf::native_gpio;

TEST(EspIdfGpioPinMapTest, PinIdIsTheNativeGpioNumber)
{
    EXPECT_EQ(native_gpio(0), 0);
    EXPECT_EQ(native_gpio(15), 15);  // SYSTEM_RELAY1, the one id the old table got right by luck
    EXPECT_EQ(native_gpio(21), 21);
}

TEST(EspIdfGpioPinMapTest, IdsAboveTheS3HoleAreNotShifted)
{
    // The retired kPinMap[] sent id 26 to GPIO 30, id 44 to GPIO 48 and made 45..48 fall off.
    EXPECT_EQ(native_gpio(26), 26);
    EXPECT_EQ(native_gpio(39), 39);  // SYSTEM_ENET_RESET
    EXPECT_EQ(native_gpio(44), 44);  // SYSTEM_MODBUS_RXD
    EXPECT_EQ(native_gpio(46), 46);  // the ticket's acceptance pin
    EXPECT_EQ(native_gpio(48), 48);  // last GPIO on the chip
}

TEST(EspIdfGpioPinMapTest, GpiosTheChipDoesNotHaveResolveToMinusOne)
{
    for (const int missing : {22, 23, 24, 25})
        EXPECT_EQ(native_gpio(missing), -1) << "GPIO " << missing;
    EXPECT_EQ(native_gpio(49), -1);
    EXPECT_EQ(native_gpio(-1), -1);
    EXPECT_EQ(native_gpio(64), -1);   // beyond the mask width: must not shift out of range
    EXPECT_EQ(native_gpio(1000), -1);
}
