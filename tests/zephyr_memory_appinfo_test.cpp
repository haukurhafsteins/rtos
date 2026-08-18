#include <array>
#include <cstddef>
#include <cstring>
#include <limits>

#include <gtest/gtest.h>

#include "rtos/AppInfo.hpp"
#include "rtos/psram.hpp"
#include "zephyr_memory_appinfo_test.hpp"

namespace
{
class ZephyrMemoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        zephyr_memory_appinfo_test::resetMemory();
    }

    void TearDown() override
    {
        zephyr_memory_appinfo_test::resetMemory();
    }
};
}

TEST_F(ZephyrMemoryTest, UsesConfiguredHeapAndReportsUsableAllocationSize)
{
    void* first = rtos::memory::psram_malloc(40);
    void* second = rtos::memory::psram_malloc(60);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(rtos::memory::psram_allocated_size(first), 40u);
    EXPECT_EQ(rtos::memory::psram_allocated_size(second), 60u);
    EXPECT_EQ(zephyr_memory_appinfo_test::allocatedBytes(), 100u);
    EXPECT_TRUE(zephyr_memory_appinfo_test::allAllocationsWereNoWait());

    rtos::memory::psram_free(first);
    rtos::memory::psram_free(second);
    EXPECT_EQ(zephyr_memory_appinfo_test::allocatedBytes(), 0u);
}

TEST_F(ZephyrMemoryTest, CallocZeroesMemoryAndRejectsSizeOverflow)
{
    auto* values = static_cast<uint32_t*>(rtos::memory::psram_calloc(8, sizeof(uint32_t)));
    ASSERT_NE(values, nullptr);
    for (std::size_t index = 0; index < 8; ++index)
        EXPECT_EQ(values[index], 0u);

    EXPECT_EQ(
        rtos::memory::psram_calloc(
            std::numeric_limits<std::size_t>::max(), 2),
        nullptr);
    EXPECT_EQ(zephyr_memory_appinfo_test::allocatedBytes(), 8 * sizeof(uint32_t));
    rtos::memory::psram_free(values);
}

TEST_F(ZephyrMemoryTest, ReallocPreservesBytesAndHonorsTheHeapBudget)
{
    auto* original = static_cast<unsigned char*>(rtos::memory::psram_malloc(100));
    ASSERT_NE(original, nullptr);
    std::memset(original, 0x5a, 100);

    auto* resized = static_cast<unsigned char*>(
        rtos::memory::psram_realloc(original, 200));
    ASSERT_NE(resized, nullptr);
    EXPECT_EQ(rtos::memory::psram_allocated_size(resized), 200u);
    for (std::size_t index = 0; index < 100; ++index)
        EXPECT_EQ(resized[index], 0x5a);

    EXPECT_EQ(rtos::memory::psram_malloc(57), nullptr);
    rtos::memory::psram_free(resized);
}

TEST(ZephyrAppInfoTest, DescriptionUsesPrimaryMcubootImageVersion)
{
    zephyr_memory_appinfo_test::setImageVersion(4, 5, 67, 890);

    const auto& description = rtos::AppInfo::description();

    EXPECT_STREQ(description.projectName, "uru_wear");
    EXPECT_STREQ(description.version, "4.5.67+890");
    EXPECT_NE(description.buildDate[0], '\0');
    EXPECT_NE(description.buildTime[0], '\0');
    EXPECT_STREQ(description.sdkVersion, "3.7.99-test");
    EXPECT_EQ(zephyr_memory_appinfo_test::lastImageAreaId(), 3u);
}

TEST(ZephyrAppInfoTest, ChipReportsStaticNrf5340Capabilities)
{
    const auto& chip = rtos::AppInfo::chip();

    EXPECT_STREQ(chip.model, "nRF5340");
    EXPECT_EQ(chip.revision, 0u);
    EXPECT_EQ(chip.cores, 2u);
    EXPECT_FALSE(chip.wifi);
    EXPECT_TRUE(chip.bluetoothLe);
    EXPECT_FALSE(chip.bluetoothClassic);
    EXPECT_TRUE(chip.ieee802154);
    EXPECT_TRUE(chip.embeddedFlash);
    EXPECT_FALSE(chip.embeddedPsram);
}

TEST(ZephyrAppInfoTest, MacAddressUsesSixHwinfoDeviceIdBytes)
{
    const std::array<uint8_t, 8> id{1, 2, 3, 4, 5, 6, 7, 8};
    zephyr_memory_appinfo_test::setDeviceId(id);
    uint8_t address[rtos::AppInfo::MacSize]{};

    EXPECT_TRUE(rtos::AppInfo::macAddress(address));
    for (std::size_t index = 0; index < rtos::AppInfo::MacSize; ++index)
        EXPECT_EQ(address[index], id[index]);
}

TEST(ZephyrAppInfoTest, MacAddressZeroesOutputWhenHwinfoFails)
{
    zephyr_memory_appinfo_test::setDeviceIdResult(-1);
    uint8_t address[rtos::AppInfo::MacSize]{1, 2, 3, 4, 5, 6};

    EXPECT_FALSE(rtos::AppInfo::macAddress(address));
    for (const auto byte : address)
        EXPECT_EQ(byte, 0u);
}
