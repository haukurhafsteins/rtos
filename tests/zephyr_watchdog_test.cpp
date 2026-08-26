#include <gtest/gtest.h>

#include "rtos/watchdog.hpp"
#include "zephyr_watchdog_test.hpp"

TEST(ZephyrWatchdogTest, InitHonorsHardwareFallbackConfiguration)
{
    zephyr_watchdog_reset_test_state();

    rtos::watchdog::Config config;
    ASSERT_TRUE(rtos::watchdog::init(config));

#if CONFIG_TASK_WDT_HW_FALLBACK
    EXPECT_EQ(zephyr_watchdog_test_device(), zephyr_watchdog_init_device());
#else
    EXPECT_EQ(nullptr, zephyr_watchdog_init_device());
#endif
}
