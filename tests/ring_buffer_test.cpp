#include <array>
#include <string>

#include <gtest/gtest.h>

#include "buffers/RingBuffer.hpp"

TEST(RingBufferTest, PushAndPopPreserveFifoOrder)
{
    StaticRingBuffer<int, 4> buffer;

    EXPECT_TRUE(buffer.push(10));
    EXPECT_TRUE(buffer.push(20));
    EXPECT_TRUE(buffer.push(30));

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer[0], 10);
    EXPECT_EQ(buffer[1], 20);
    EXPECT_EQ(buffer[2], 30);
    EXPECT_EQ(buffer.getRecent(0), 30);
    EXPECT_EQ(buffer.getRecent(1), 20);

    int first = 0;
    int second = 0;
    EXPECT_TRUE(buffer.pop(first));
    EXPECT_TRUE(buffer.pop(second));

    EXPECT_EQ(first, 10);
    EXPECT_EQ(second, 20);
    EXPECT_EQ(buffer.size(), 1u);
    EXPECT_EQ(buffer.getLast(), 30);
}

TEST(RingBufferTest, PushFailsWhenBufferIsFull)
{
    StaticRingBuffer<int, 3> buffer;

    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.push(3));
    EXPECT_TRUE(buffer.isFull());
    EXPECT_FALSE(buffer.push(4));

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
}

TEST(RingBufferTest, PushOverwriteDropsOldestEntries)
{
    StaticRingBuffer<int, 3> buffer;

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push_overwrite(4);
    buffer.push_overwrite(5);

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer[0], 3);
    EXPECT_EQ(buffer[1], 4);
    EXPECT_EQ(buffer[2], 5);
    EXPECT_EQ(buffer.getRecent(0), 5);
    EXPECT_EQ(buffer.oldestIndex(), 2u);
}

TEST(RingBufferTest, CopyOutAndPopNHandleWrappedStorage)
{
    StaticRingBuffer<int, 5> buffer;
    for (int value = 1; value <= 5; ++value)
    {
        ASSERT_TRUE(buffer.push(value));
    }

    int discarded = 0;
    ASSERT_TRUE(buffer.pop(discarded));
    ASSERT_TRUE(buffer.pop(discarded));
    ASSERT_TRUE(buffer.push(6));
    ASSERT_TRUE(buffer.push(7));

    std::array<int, 5> copied{};
    const std::size_t copiedCount = buffer.copy_out(copied.data(), copied.size());
    EXPECT_EQ(copiedCount, 5u);
    EXPECT_EQ(copied, (std::array<int, 5>{3, 4, 5, 6, 7}));

    std::array<int, 3> popped{};
    const std::size_t poppedCount = buffer.pop_n(popped.data(), popped.size());
    EXPECT_EQ(poppedCount, 3u);
    EXPECT_EQ(popped, (std::array<int, 3>{3, 4, 5}));
    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], 6);
    EXPECT_EQ(buffer[1], 7);
}

TEST(RingBufferTest, PeekSpanReturnsContiguousOldestSegment)
{
    StaticRingBuffer<int, 5> buffer;
    for (int value = 1; value <= 5; ++value)
    {
        ASSERT_TRUE(buffer.push(value));
    }

    int discarded = 0;
    ASSERT_TRUE(buffer.pop(discarded));
    ASSERT_TRUE(buffer.pop(discarded));
    ASSERT_TRUE(buffer.push(6));
    ASSERT_TRUE(buffer.push(7));

    const auto [data, length] = buffer.peek_span();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(length, 3u);
    EXPECT_EQ(data[0], 3);
    EXPECT_EQ(data[1], 4);
    EXPECT_EQ(data[2], 5);
}

TEST(RingBufferTest, ToJsonSerializesRequestedWindow)
{
    StaticRingBuffer<float, 4> buffer;
    buffer.push(1.25f);
    buffer.push(2.5f);
    buffer.push(3.75f);

    std::array<char, 64> json{};
    const int written = buffer.toJson(json, 3);

    ASSERT_GT(written, 0);
    EXPECT_EQ(std::string(json.data(), static_cast<std::size_t>(written)), "[1.25,2.5,3.75]");
    EXPECT_EQ(buffer.toJson(json, 4), -1);
}