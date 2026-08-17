#include <algorithm>
#include <csetjmp>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rtos/Log.hpp"
#include "rtos/backend.hpp"
#include "zephyr/kernel.h"

namespace
{
struct ThreadCreateCall
{
    k_thread *thread = nullptr;
    k_thread_stack_t *stack = nullptr;
    std::size_t stackSize = 0;
    k_thread_entry_t entry = nullptr;
    void *p1 = nullptr;
    void *p2 = nullptr;
    void *p3 = nullptr;
    int priority = -1;
    std::uint32_t options = 0;
    k_timeout_t delay{};
    std::string name;
};

struct FakeQueueState
{
    std::size_t itemSize = 0;
    std::uint32_t length = 0;
    std::deque<std::vector<unsigned char>> items;
};

std::vector<ThreadCreateCall> s_threadCreates;
std::vector<k_tid_t> s_abortedThreads;
k_tid_t s_currentThread = nullptr;
int s_yieldCount = 0;
k_timeout_t s_lastSleep{};
k_timeout_t s_lastQueueTimeout{};
bool s_failThreadCreate = false;
bool s_failQueueInit = false;
bool s_abortSelfDoesNotReturn = false;
std::jmp_buf s_selfAbortJump;
int s_userTaskCalls = 0;
void *s_userTaskArgument = nullptr;
int s_logCalls = 0;
rtos::LogLevel s_lastLogLevel = rtos::LogLevel::None;
std::string s_lastLogTag;
std::string s_lastLogMessage;

FakeQueueState *queueState(k_msgq *queue)
{
    return static_cast<FakeQueueState *>(queue->state);
}

void userTask(void *argument)
{
    ++s_userTaskCalls;
    s_userTaskArgument = argument;
}

class ZephyrTaskQueueTest : public testing::Test
{
protected:
    void SetUp() override
    {
        s_threadCreates.clear();
        s_abortedThreads.clear();
        s_currentThread = nullptr;
        s_yieldCount = 0;
        s_lastSleep = {};
        s_lastQueueTimeout = {};
        s_failThreadCreate = false;
        s_failQueueInit = false;
        s_abortSelfDoesNotReturn = false;
        s_userTaskCalls = 0;
        s_userTaskArgument = nullptr;
        s_logCalls = 0;
        s_lastLogLevel = rtos::LogLevel::None;
        s_lastLogTag.clear();
        s_lastLogMessage.clear();
    }

    void TearDown() override
    {
        for (auto handle : taskHandles)
        {
            if (handle)
                rtos::backend::task_delete(handle);
        }
        for (auto handle : queueHandles)
        {
            if (handle)
                rtos::backend::queue_delete(handle);
        }
    }

    rtos::backend::TaskHandle createTask(
        std::uint32_t priority = 5,
        std::uint32_t stackBytes = 4096)
    {
        rtos::backend::TaskHandle handle = nullptr;
        if (rtos::backend::task_create(
                handle, "worker", stackBytes, priority, userTask,
                reinterpret_cast<void *>(0x1234)))
        {
            taskHandles.push_back(handle);
        }
        return handle;
    }

    rtos::backend::QueueHandle createQueue(std::size_t length, std::size_t itemSize)
    {
        rtos::backend::QueueHandle handle = nullptr;
        if (rtos::backend::queue_create(handle, length, itemSize))
            queueHandles.push_back(handle);
        return handle;
    }

    void forgetTask(rtos::backend::TaskHandle handle)
    {
        auto found = std::find(taskHandles.begin(), taskHandles.end(), handle);
        if (found != taskHandles.end())
            *found = nullptr;
    }

    std::vector<rtos::backend::TaskHandle> taskHandles;
    std::vector<rtos::backend::QueueHandle> queueHandles;
};
}

namespace rtos
{
void Log::log(LogLevel level, const char *tag, const char *format, ...)
{
    char message[256]{};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    ++s_logCalls;
    s_lastLogLevel = level;
    s_lastLogTag = tag ? tag : "";
    s_lastLogMessage = message;
}
}

extern "C" k_tid_t k_thread_create(
    k_thread *thread,
    k_thread_stack_t *stack,
    std::size_t stackSize,
    k_thread_entry_t entry,
    void *p1,
    void *p2,
    void *p3,
    int priority,
    std::uint32_t options,
    k_timeout_t delay)
{
    if (s_failThreadCreate)
        return nullptr;
    thread->identifier = static_cast<int>(s_threadCreates.size()) + 1;
    s_threadCreates.push_back(
        {thread, stack, stackSize, entry, p1, p2, p3, priority, options, delay, {}});
    return thread;
}

extern "C" int k_thread_name_set(k_tid_t thread, const char *name)
{
    for (auto &call : s_threadCreates)
    {
        if (call.thread == thread)
        {
            call.name = name ? name : "";
            break;
        }
    }
    return 0;
}

extern "C" void k_thread_abort(k_tid_t thread)
{
    s_abortedThreads.push_back(thread);
    if (s_abortSelfDoesNotReturn && thread == s_currentThread)
        std::longjmp(s_selfAbortJump, 1);
}

extern "C" k_tid_t k_current_get()
{
    return s_currentThread;
}

extern "C" int k_sleep(k_timeout_t timeout)
{
    s_lastSleep = timeout;
    return 0;
}

extern "C" void k_yield()
{
    ++s_yieldCount;
}

extern "C" k_spinlock_key_t k_spin_lock(k_spinlock *)
{
    return 0;
}

extern "C" void k_spin_unlock(k_spinlock *, k_spinlock_key_t)
{
}

extern "C" void *k_malloc(std::size_t size)
{
    return std::malloc(size);
}

extern "C" void k_free(void *memory)
{
    std::free(memory);
}

extern "C" int k_msgq_alloc_init(k_msgq *queue, std::size_t itemSize, std::uint32_t length)
{
    if (s_failQueueInit)
        return -1;
    queue->state = new FakeQueueState{itemSize, length, {}};
    return 0;
}

extern "C" int k_msgq_cleanup(k_msgq *queue)
{
    delete queueState(queue);
    queue->state = nullptr;
    return 0;
}

extern "C" int k_msgq_put(k_msgq *queue, const void *item, k_timeout_t timeout)
{
    s_lastQueueTimeout = timeout;
    auto *state = queueState(queue);
    if (state->items.size() >= state->length)
        return -1;
    const auto *bytes = static_cast<const unsigned char *>(item);
    state->items.emplace_back(bytes, bytes + state->itemSize);
    return 0;
}

extern "C" int k_msgq_get(k_msgq *queue, void *item, k_timeout_t timeout)
{
    s_lastQueueTimeout = timeout;
    auto *state = queueState(queue);
    if (state->items.empty())
        return -1;
    std::memcpy(item, state->items.front().data(), state->itemSize);
    state->items.pop_front();
    return 0;
}

extern "C" void k_msgq_purge(k_msgq *queue)
{
    queueState(queue)->items.clear();
}

extern "C" std::uint32_t k_msgq_num_free_get(k_msgq *queue)
{
    auto *state = queueState(queue);
    return state->length - static_cast<std::uint32_t>(state->items.size());
}

extern "C" std::uint32_t k_msgq_num_used_get(k_msgq *queue)
{
    return static_cast<std::uint32_t>(queueState(queue)->items.size());
}

TEST_F(ZephyrTaskQueueTest, CreatesTaskWithTranslatedPriorityAndTrampoline)
{
    auto handle = createTask(5);

    ASSERT_NE(handle, nullptr);
    ASSERT_EQ(s_threadCreates.size(), 1u);
    const auto &call = s_threadCreates.front();
    EXPECT_EQ(call.thread, handle);
    EXPECT_GE(call.stackSize, 4096u);
    EXPECT_EQ(call.priority, K_PRIO_PREEMPT(24 - 5));
    EXPECT_EQ(call.options, 0u);
    EXPECT_EQ(call.delay.milliseconds, K_NO_WAIT.milliseconds);
    EXPECT_EQ(call.name, "worker");

    call.entry(call.p1, call.p2, call.p3);
    EXPECT_EQ(s_userTaskCalls, 1);
    EXPECT_EQ(s_userTaskArgument, reinterpret_cast<void *>(0x1234));
}

TEST_F(ZephyrTaskQueueTest, RejectsPriorityOutsideConfiguredPreemptiveRange)
{
    auto lowest = createTask(0);
    ASSERT_NE(lowest, nullptr);
    EXPECT_EQ(s_threadCreates.back().priority, K_PRIO_PREEMPT(24));

    auto highest = createTask(24);
    ASSERT_NE(highest, nullptr);
    EXPECT_EQ(s_threadCreates.back().priority, K_PRIO_PREEMPT(0));

    rtos::backend::TaskHandle invalid = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::task_create(
        invalid, "invalid", 4096, 25, userTask, nullptr));
    EXPECT_EQ(invalid, nullptr);
}

TEST_F(ZephyrTaskQueueTest, ReusesOneOfSixteenStaticTaskSlotsAfterDelete)
{
    for (int index = 0; index < 16; ++index)
        ASSERT_NE(createTask(), nullptr);

    rtos::backend::TaskHandle overflow = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::task_create(
        overflow, "overflow", 4096, 5, userTask, nullptr));
    EXPECT_EQ(overflow, nullptr);

    auto released = taskHandles.front();
    rtos::backend::task_delete(released);
    forgetTask(released);
    EXPECT_NE(createTask(), nullptr);
}

TEST_F(ZephyrTaskQueueTest, ReusesStaticTaskSlotWhenTaskFunctionReturns)
{
    for (int index = 0; index < 16; ++index)
        ASSERT_NE(createTask(), nullptr);

    const auto finished = taskHandles.front();
    const auto &call = s_threadCreates.front();
    call.entry(call.p1, call.p2, call.p3);
    forgetTask(finished);

    EXPECT_NE(createTask(), nullptr);
}

TEST_F(ZephyrTaskQueueTest, ReleasesStaticTaskSlotBeforeSelfAbortDoesNotReturn)
{
    for (int index = 0; index < 16; ++index)
        ASSERT_NE(createTask(), nullptr);

    const auto self = taskHandles.front();
    s_currentThread = static_cast<k_tid_t>(self);
    s_abortSelfDoesNotReturn = true;
    if (setjmp(s_selfAbortJump) == 0)
    {
        rtos::backend::task_delete(self);
        FAIL() << "self-abort unexpectedly returned";
    }

    s_abortSelfDoesNotReturn = false;
    s_currentThread = nullptr;
    forgetTask(self);
    const auto replacement = createTask();
    EXPECT_NE(replacement, nullptr);
    if (!replacement)
        rtos::backend::task_delete(self);
}

TEST_F(ZephyrTaskQueueTest, PinnedCreateDocumentsSingleCoreNeutrality)
{
    rtos::backend::TaskHandle handle = nullptr;
    EXPECT_TRUE(rtos::backend::task_create_pinned(
        handle, "pinned", 4096, 7, 99, userTask, nullptr));
    ASSERT_NE(handle, nullptr);
    taskHandles.push_back(handle);
    EXPECT_EQ(s_threadCreates.back().priority, K_PRIO_PREEMPT(24 - 7));
}

TEST_F(ZephyrTaskQueueTest, ExposesDelayYieldCurrentAndDelete)
{
    auto handle = createTask();
    ASSERT_NE(handle, nullptr);
    s_currentThread = static_cast<k_tid_t>(handle);

    rtos::backend::delay_ms(rtos::Millis(37));
    rtos::backend::yield();
    EXPECT_EQ(rtos::backend::current_task(), handle);
    EXPECT_EQ(s_lastSleep.milliseconds, 37);
    EXPECT_EQ(s_yieldCount, 1);

    rtos::backend::task_delete(handle);
    forgetTask(handle);
    ASSERT_EQ(s_abortedThreads.size(), 1u);
    EXPECT_EQ(s_abortedThreads.front(), handle);
}

TEST_F(ZephyrTaskQueueTest, ReportsThreadAndStackAdmissionFailures)
{
    EXPECT_EQ(createTask(5, 1024 * 1024), nullptr);

    s_failThreadCreate = true;
    EXPECT_EQ(createTask(), nullptr);

    s_failThreadCreate = false;
    EXPECT_NE(createTask(), nullptr);
}

TEST_F(ZephyrTaskQueueTest, LogsRejectedPriorityWithTaskNameAndCeiling)
{
    rtos::backend::TaskHandle handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::task_create(
        handle, "vibrate", 4096, 25, userTask, nullptr));
    EXPECT_EQ(handle, nullptr);

    ASSERT_EQ(s_logCalls, 1);
    EXPECT_EQ(s_lastLogLevel, rtos::LogLevel::Error);
    EXPECT_EQ(s_lastLogTag, "rtos.task");
    EXPECT_NE(s_lastLogMessage.find("vibrate"), std::string::npos);
    EXPECT_NE(s_lastLogMessage.find("25"), std::string::npos);
    EXPECT_NE(s_lastLogMessage.find("24"), std::string::npos);
}

TEST_F(ZephyrTaskQueueTest, QueueRoundTripsFixedSizeItemsAndReportsCapacity)
{
    auto handle = createQueue(2, sizeof(std::uint32_t));
    ASSERT_NE(handle, nullptr);
    std::uint32_t first = 11;
    std::uint32_t second = 22;
    std::uint32_t third = 33;

    EXPECT_TRUE(rtos::backend::queue_send(handle, &first, 0));
    EXPECT_TRUE(rtos::backend::queue_send(handle, &second, 17));
    EXPECT_FALSE(rtos::backend::queue_send(handle, &third, 17));
    EXPECT_EQ(s_lastQueueTimeout.milliseconds, 17);
    EXPECT_EQ(rtos::backend::queue_count(handle), 2u);
    EXPECT_EQ(rtos::backend::queue_spaces(handle), 0u);

    std::uint32_t received = 0;
    EXPECT_TRUE(rtos::backend::queue_receive(handle, &received, 23));
    EXPECT_EQ(received, first);
    EXPECT_EQ(s_lastQueueTimeout.milliseconds, 23);
    EXPECT_EQ(rtos::backend::queue_count(handle), 1u);
    EXPECT_EQ(rtos::backend::queue_spaces(handle), 1u);

    EXPECT_TRUE(rtos::backend::queue_reset(handle));
    EXPECT_EQ(rtos::backend::queue_count(handle), 0u);
}

TEST_F(ZephyrTaskQueueTest, QueueMapsWaitForeverAndIsrCallsUseNoWait)
{
    auto handle = createQueue(1, sizeof(std::uint32_t));
    ASSERT_NE(handle, nullptr);
    std::uint32_t value = 9;

    EXPECT_TRUE(rtos::backend::queue_send(
        handle, &value, std::numeric_limits<std::uint32_t>::max()));
    EXPECT_EQ(s_lastQueueTimeout.milliseconds, K_FOREVER.milliseconds);

    bool taskWoken = true;
    EXPECT_FALSE(rtos::backend::queue_send_isr(handle, &value, &taskWoken));
    EXPECT_FALSE(taskWoken);
    EXPECT_EQ(s_lastQueueTimeout.milliseconds, K_NO_WAIT.milliseconds);

    std::uint32_t received = 0;
    taskWoken = true;
    EXPECT_TRUE(rtos::backend::queue_receive_isr(handle, &received, &taskWoken));
    EXPECT_EQ(received, value);
    EXPECT_FALSE(taskWoken);
    EXPECT_EQ(s_lastQueueTimeout.milliseconds, K_NO_WAIT.milliseconds);
}

TEST_F(ZephyrTaskQueueTest, QueueCreationValidatesShapeAndAllocation)
{
    rtos::backend::QueueHandle handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::queue_create(handle, 0, sizeof(std::uint32_t)));
    EXPECT_EQ(handle, nullptr);

    handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::queue_create(handle, 1, 0));
    EXPECT_EQ(handle, nullptr);

    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t))
    {
        handle = reinterpret_cast<void *>(0x1);
        EXPECT_FALSE(rtos::backend::queue_create(
            handle,
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1,
            sizeof(std::uint32_t)));
        EXPECT_EQ(handle, nullptr);
    }

    s_failQueueInit = true;
    handle = reinterpret_cast<void *>(0x1);
    EXPECT_FALSE(rtos::backend::queue_create(handle, 1, sizeof(std::uint32_t)));
    EXPECT_EQ(handle, nullptr);
}
