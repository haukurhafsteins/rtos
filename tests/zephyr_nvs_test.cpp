#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "rtos/Nvs.hpp"
#include "zephyr_nvs_test.hpp"

namespace
{
class ZephyrNvsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rtos::nvs::deinit();
        zephyr_nvs_test::reset();
        ASSERT_TRUE(rtos::nvs::init());
    }

    void TearDown() override
    {
        rtos::nvs::deinit();
    }
};
}

TEST_F(ZephyrNvsTest, SameTypeAndSizeDoesNotRewriteCollisionTable)
{
    rtos::Nvs values;
    ASSERT_TRUE(values.open("values"));
    ASSERT_TRUE(values.set_u32("count", 1));
    ASSERT_TRUE(values.set_u32("count", 2));

    EXPECT_EQ(zephyr_nvs_test::writeCount(0), 1u);
}

TEST_F(ZephyrNvsTest, EraseReclaimsMappingAcrossRestarts)
{
    for (unsigned index = 0; index < 65; ++index)
    {
        rtos::Nvs values;
        ASSERT_TRUE(values.open("values"));
        const std::string key = "key" + std::to_string(index);
        ASSERT_TRUE(values.set_u32(key.c_str(), index)) << index;
        ASSERT_TRUE(values.erase_key(key.c_str())) << index;
        values.close();

        rtos::nvs::deinit();
        ASSERT_TRUE(rtos::nvs::init()) << index;
    }
}

TEST_F(ZephyrNvsTest, FailedEraseTableWriteRollsBackMapping)
{
    rtos::Nvs values;
    ASSERT_TRUE(values.open("values"));
    ASSERT_TRUE(values.set_u32("count", 1));

    zephyr_nvs_test::failNextWrite(0);
    EXPECT_FALSE(values.erase_key("count"));
    EXPECT_TRUE(values.set_u32("count", 2));
    EXPECT_EQ(zephyr_nvs_test::writeCount(0), 2u);
}
