#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "rtos/backend.hpp"
#include "zephyr/kernel.h"

namespace
{
std::vector<k_timeout_t> s_semTakeTimeouts;
int s_allocationsBeforeFailure = -1;
std::int64_t s_uptimeMilliseconds = 1000;

class ZephyrMessageBufferTest : public testing::Test
{
protected:
    void SetUp() override
    {
        s_semTakeTimeouts.clear();
        s_allocationsBeforeFailure = -1;
        s_uptimeMilliseconds = 1000;
    }

    void TearDown() override
    {
        for (auto handle : handles)
        {
            if (handle)
                rtos::backend::msgbuf_delete(handle);
        }
    }

    rtos::backend::MsgBufferHandle create(std::size_t capacity)
    {
        rtos::backend::MsgBufferHandle handle = nullptr;
        if (rtos::backend::msgbuf_create(handle, capacity))
            handles.push_back(handle);
        return handle;
    }

    std::vector<rtos::backend::MsgBufferHandle> handles;
};
}

extern "C" k_spinlock_key_t k_spin_lock(k_spinlock *)
{
    return 0;
}

extern "C" void k_spin_unlock(k_spinlock *, k_spinlock_key_t)
{
}

extern "C" int k_sem_init(
    k_sem *sem, unsigned int initialCount, unsigned int limit)
{
    sem->count = initialCount;
    sem->limit = limit;
    return 0;
}

extern "C" int k_sem_take(k_sem *sem, k_timeout_t timeout)
{
    s_semTakeTimeouts.push_back(timeout);
    if (sem->count == 0)
        return -1;
    --sem->count;
    return 0;
}

extern "C" void k_sem_give(k_sem *sem)
{
    if (sem->count < sem->limit)
        ++sem->count;
}

extern "C" void k_sem_reset(k_sem *sem)
{
    sem->count = 0;
}

extern "C" void *k_malloc(std::size_t size)
{
    if (s_allocationsBeforeFailure == 0)
        return nullptr;
    if (s_allocationsBeforeFailure > 0)
        --s_allocationsBeforeFailure;
    return std::malloc(size);
}

extern "C" void k_free(void *memory)
{
    std::free(memory);
}

extern "C" std::int64_t k_uptime_get()
{
    return s_uptimeMilliseconds;
}

TEST_F(ZephyrMessageBufferTest, RoundTripsMessagesAndReportsNextLength)
{
    auto handle = create(13);
    ASSERT_NE(handle, nullptr);
    const std::array<std::uint8_t, 3> first{1, 2, 3};
    const std::array<std::uint8_t, 5> second{9, 8, 7, 6, 5};

    EXPECT_EQ(rtos::backend::msgbuf_send(
        handle, first.data(), first.size(), rtos::Millis(0)), first.size());
    EXPECT_EQ(rtos::backend::msgbuf_send(
        handle, second.data(), second.size(), rtos::Millis(0)), second.size());
    EXPECT_EQ(rtos::backend::msgbuf_next_len(handle), first.size());

    std::array<std::uint8_t, 8> output{};
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), first.size());
    EXPECT_TRUE(std::equal(first.begin(), first.end(), output.begin()));
    EXPECT_EQ(rtos::backend::msgbuf_next_len(handle), second.size());

    output.fill(0);
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), second.size());
    EXPECT_TRUE(std::equal(second.begin(), second.end(), output.begin()));
    EXPECT_EQ(rtos::backend::msgbuf_next_len(handle), 0u);

    ASSERT_EQ(rtos::backend::msgbuf_send(
        handle, first.data(), first.size(), rtos::Millis(0)), first.size());
    output.fill(0);
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), first.size());
    EXPECT_TRUE(std::equal(first.begin(), first.end(), output.begin()));
}

TEST_F(ZephyrMessageBufferTest, ReportsSpaceIncludingTwoByteFramePrefix)
{
    auto handle = create(10);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(rtos::backend::msgbuf_space_available(handle), 10u);

    const std::array<std::uint8_t, 4> payload{1, 2, 3, 4};
    ASSERT_EQ(rtos::backend::msgbuf_send(
        handle, payload.data(), payload.size(), rtos::Millis(0)), payload.size());
    EXPECT_EQ(rtos::backend::msgbuf_space_available(handle), 4u);

    std::array<std::uint8_t, 4> output{};
    ASSERT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), payload.size());
    EXPECT_EQ(rtos::backend::msgbuf_space_available(handle), 10u);
}

TEST_F(ZephyrMessageBufferTest, IsrVariantsRoundTripWithoutWaiting)
{
    auto handle = create(8);
    ASSERT_NE(handle, nullptr);
    const std::array<std::uint8_t, 3> payload{4, 5, 6};

    bool taskWoken = true;
    EXPECT_EQ(rtos::backend::msgbuf_send_isr(
        handle, payload.data(), payload.size(), &taskWoken), payload.size());
    EXPECT_FALSE(taskWoken);

    std::array<std::uint8_t, 3> output{};
    taskWoken = true;
    EXPECT_EQ(rtos::backend::msgbuf_receive_isr(
        handle, output.data(), output.size(), &taskWoken), payload.size());
    EXPECT_FALSE(taskWoken);
    EXPECT_EQ(output, payload);

    taskWoken = true;
    EXPECT_EQ(rtos::backend::msgbuf_receive_isr(
        handle, output.data(), output.size(), &taskWoken), 0u);
    EXPECT_FALSE(taskWoken);
    EXPECT_TRUE(s_semTakeTimeouts.empty());
}

TEST_F(ZephyrMessageBufferTest, ResetDropsMessagesAndRestoresCapacity)
{
    auto handle = create(12);
    ASSERT_NE(handle, nullptr);
    const std::array<std::uint8_t, 3> payload{7, 8, 9};
    ASSERT_EQ(rtos::backend::msgbuf_send(
        handle, payload.data(), payload.size(), rtos::Millis(0)), payload.size());

    EXPECT_TRUE(rtos::backend::msgbuf_reset(handle));
    EXPECT_EQ(rtos::backend::msgbuf_next_len(handle), 0u);
    EXPECT_EQ(rtos::backend::msgbuf_space_available(handle), 12u);
    std::array<std::uint8_t, 3> output{};
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), 0u);
}

TEST_F(ZephyrMessageBufferTest, KeepsMessageWhenReceiveBufferIsTooSmall)
{
    auto handle = create(12);
    ASSERT_NE(handle, nullptr);
    const std::array<std::uint8_t, 5> payload{1, 3, 5, 7, 9};
    ASSERT_EQ(rtos::backend::msgbuf_send(
        handle, payload.data(), payload.size(), rtos::Millis(0)), payload.size());

    std::array<std::uint8_t, 4> tooSmall{};
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, tooSmall.data(), tooSmall.size(), rtos::Millis(0)), 0u);
    EXPECT_EQ(rtos::backend::msgbuf_next_len(handle), payload.size());

    std::array<std::uint8_t, 5> output{};
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), payload.size());
    EXPECT_EQ(output, payload);
}

TEST_F(ZephyrMessageBufferTest, MapsFiniteAndForeverWaitsToSemaphores)
{
    auto handle = create(6);
    ASSERT_NE(handle, nullptr);
    std::array<std::uint8_t, 4> payload{1, 2, 3, 4};
    ASSERT_EQ(rtos::backend::msgbuf_send(
        handle, payload.data(), payload.size(), rtos::Millis(0)), payload.size());

    EXPECT_EQ(rtos::backend::msgbuf_send(
        handle, payload.data(), 1, rtos::Millis(17)), 0u);
    ASSERT_FALSE(s_semTakeTimeouts.empty());
    EXPECT_EQ(s_semTakeTimeouts.back().milliseconds, 17);

    std::array<std::uint8_t, 4> output{};
    ASSERT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis(0)), payload.size());
    EXPECT_EQ(rtos::backend::msgbuf_receive(
        handle, output.data(), output.size(), rtos::Millis::max()), 0u);
    EXPECT_EQ(s_semTakeTimeouts.back().milliseconds, K_FOREVER.milliseconds);
}

TEST_F(ZephyrMessageBufferTest, ValidatesCapacityLengthAndAllocation)
{
    rtos::backend::MsgBufferHandle handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::msgbuf_create(handle, 1));
    EXPECT_EQ(handle, nullptr);

    auto valid = create(8);
    ASSERT_NE(valid, nullptr);
    std::uint8_t byte = 1;
    EXPECT_EQ(rtos::backend::msgbuf_send(
        valid, &byte, 0, rtos::Millis(0)), 0u);
    EXPECT_EQ(rtos::backend::msgbuf_send(
        valid, &byte,
        static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1,
        rtos::Millis(0)), 0u);

    s_allocationsBeforeFailure = 0;
    handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::msgbuf_create(handle, 8));
    EXPECT_EQ(handle, nullptr);

    s_allocationsBeforeFailure = 1;
    handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::msgbuf_create(handle, 8));
    EXPECT_EQ(handle, nullptr);
}
