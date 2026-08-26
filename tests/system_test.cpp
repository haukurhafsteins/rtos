#include <gtest/gtest.h>

#include "rtos/system.hpp"

TEST(SystemTest, RestartTerminatesTheHostProcess)
{
    EXPECT_DEATH(rtos::system::restart(), "");
}
