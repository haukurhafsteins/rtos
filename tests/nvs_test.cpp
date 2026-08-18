#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include <gtest/gtest.h>

#include "rtos/Nvs.hpp"

namespace
{
class NvsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(rtos::nvs::init());
        ASSERT_TRUE(rtos::nvs::erase());
        ASSERT_TRUE(rtos::nvs::init());
    }

    void TearDown() override
    {
        rtos::nvs::deinit();
    }
};
}

TEST_F(NvsTest, PersistsAllValueTypesAcrossHandles)
{
    constexpr std::array<std::byte, 5> blob{
        std::byte{0x00}, std::byte{0x17}, std::byte{0x80},
        std::byte{0xfe}, std::byte{0xff}};

    rtos::Nvs writer;
    ASSERT_TRUE(writer.open("device"));
    EXPECT_TRUE(writer.set_str("name", "URU Nordic"));
    EXPECT_TRUE(writer.set_blob("blob", blob.data(), blob.size()));
    EXPECT_TRUE(writer.set_u32("unsigned", UINT32_C(0xfedcba98)));
    EXPECT_TRUE(writer.set_i32("signed", INT32_C(-1234567)));
    EXPECT_TRUE(writer.commit());
    writer.close();

    rtos::Nvs reader;
    ASSERT_TRUE(reader.open("device", rtos::Nvs::Mode::ReadOnly));

    std::array<char, 32> text{};
    std::size_t textSize = text.size();
    EXPECT_TRUE(reader.get_str("name", text.data(), textSize));
    EXPECT_STREQ(text.data(), "URU Nordic");
    EXPECT_EQ(textSize, std::strlen("URU Nordic") + 1);

    std::array<std::byte, 8> readBlob{};
    std::size_t blobSize = readBlob.size();
    EXPECT_TRUE(reader.get_blob("blob", readBlob.data(), blobSize));
    EXPECT_EQ(blobSize, blob.size());
    EXPECT_TRUE(std::equal(blob.begin(), blob.end(), readBlob.begin()));

    uint32_t unsignedValue = 0;
    int32_t signedValue = 0;
    EXPECT_TRUE(reader.get_u32("unsigned", unsignedValue));
    EXPECT_TRUE(reader.get_i32("signed", signedValue));
    EXPECT_EQ(unsignedValue, UINT32_C(0xfedcba98));
    EXPECT_EQ(signedValue, INT32_C(-1234567));
}

TEST_F(NvsTest, KeepsNamespacesAndStoredTypesDistinct)
{
    rtos::Nvs first;
    rtos::Nvs second;
    ASSERT_TRUE(first.open("first"));
    ASSERT_TRUE(second.open("second"));
    ASSERT_TRUE(first.set_u32("shared", 11));
    ASSERT_TRUE(second.set_u32("shared", 22));
    ASSERT_TRUE(first.set_blob("typed", "x", 2));

    uint32_t firstValue = 0;
    uint32_t secondValue = 0;
    EXPECT_TRUE(first.get_u32("shared", firstValue));
    EXPECT_TRUE(second.get_u32("shared", secondValue));
    EXPECT_EQ(firstValue, 11u);
    EXPECT_EQ(secondValue, 22u);

    uint32_t wrongType = 0;
    EXPECT_FALSE(first.get_u32("typed", wrongType));
}

TEST_F(NvsTest, ReadOnlyHandleRejectsEveryMutation)
{
    rtos::Nvs writer;
    ASSERT_TRUE(writer.open("config"));
    ASSERT_TRUE(writer.set_u32("key", 7));
    writer.close();

    rtos::Nvs reader;
    ASSERT_TRUE(reader.open("config", rtos::Nvs::Mode::ReadOnly));
    const std::array<std::byte, 2> blob{std::byte{1}, std::byte{2}};
    EXPECT_FALSE(reader.set_str("string", "value"));
    EXPECT_FALSE(reader.set_blob("blob", blob.data(), blob.size()));
    EXPECT_FALSE(reader.set_u32("key", 8));
    EXPECT_FALSE(reader.set_i32("signed", -1));
    EXPECT_FALSE(reader.erase_key("key"));
    EXPECT_TRUE(reader.commit());

    uint32_t value = 0;
    EXPECT_TRUE(reader.get_u32("key", value));
    EXPECT_EQ(value, 7u);
}

TEST_F(NvsTest, ErasesIndividualKeysAndTheWholeStore)
{
    rtos::Nvs values;
    ASSERT_TRUE(values.open("values"));
    ASSERT_TRUE(values.set_u32("keep", 1));
    ASSERT_TRUE(values.set_u32("remove", 2));
    EXPECT_TRUE(values.erase_key("remove"));

    uint32_t value = 0;
    EXPECT_FALSE(values.get_u32("remove", value));
    EXPECT_TRUE(values.get_u32("keep", value));
    EXPECT_EQ(value, 1u);
    values.close();

    ASSERT_TRUE(rtos::nvs::erase());
    ASSERT_TRUE(rtos::nvs::init());

    rtos::Nvs empty;
    EXPECT_FALSE(empty.open("values", rtos::Nvs::Mode::ReadOnly));
}

TEST_F(NvsTest, ReportsRequiredSizesWithoutOverwritingSmallBuffers)
{
    rtos::Nvs values;
    ASSERT_TRUE(values.open("sizes"));
    ASSERT_TRUE(values.set_str("string", "abcd"));
    const std::array<std::byte, 4> blob{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    ASSERT_TRUE(values.set_blob("blob", blob.data(), blob.size()));

    std::array<char, 3> text{'x', 'x', '\0'};
    std::size_t textSize = text.size();
    EXPECT_FALSE(values.get_str("string", text.data(), textSize));
    EXPECT_EQ(textSize, 5u);
    EXPECT_STREQ(text.data(), "xx");

    std::array<std::byte, 2> output{std::byte{9}, std::byte{9}};
    std::size_t blobSize = output.size();
    EXPECT_FALSE(values.get_blob("blob", output.data(), blobSize));
    EXPECT_EQ(blobSize, blob.size());
    EXPECT_EQ(output[0], std::byte{9});
    EXPECT_EQ(output[1], std::byte{9});
}

TEST_F(NvsTest, MoveTransfersTheOpenNamespace)
{
    rtos::Nvs source;
    ASSERT_TRUE(source.open("move"));
    ASSERT_TRUE(source.set_u32("key", 42));

    rtos::Nvs destination(std::move(source));
    EXPECT_FALSE(source.opened());
    EXPECT_TRUE(destination.opened());

    uint32_t value = 0;
    EXPECT_TRUE(destination.get_u32("key", value));
    EXPECT_EQ(value, 42u);
}
