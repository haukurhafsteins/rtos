#include <gtest/gtest.h>

#include "rtos/system.hpp"
#include "zephyr/sys/reboot.h"

namespace
{
struct RebootRequested
{};

int rebootMode = -1;
}

extern "C" void sys_reboot(int mode)
{
    rebootMode = mode;
    throw RebootRequested{};
}

TEST(ZephyrSystemTest, RestartRequestsAColdReboot)
{
    rebootMode = -1;

    EXPECT_THROW(rtos::system::restart(), RebootRequested);
    EXPECT_EQ(rebootMode, SYS_REBOOT_COLD);
}
