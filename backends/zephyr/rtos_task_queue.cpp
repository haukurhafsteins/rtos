#include "rtos/backend.hpp"
#include "rtos/Log.hpp"

#include <cstdint>
#include <limits>

#include <zephyr/kernel.h>

#ifndef RTOS_ZEPHYR_MAX_TASKS
#define RTOS_ZEPHYR_MAX_TASKS 16
#endif

#ifndef RTOS_ZEPHYR_MAX_TASK_STACK_BYTES
#define RTOS_ZEPHYR_MAX_TASK_STACK_BYTES (10U * 1024U)
#endif

namespace
{
using rtos::TaskFunction;

constexpr std::size_t TASK_SLOT_COUNT = RTOS_ZEPHYR_MAX_TASKS;
constexpr std::size_t TASK_STACK_BYTES = RTOS_ZEPHYR_MAX_TASK_STACK_BYTES;
constexpr std::uint32_t PRIO_CEIL = CONFIG_NUM_PREEMPT_PRIORITIES - 1U;

static_assert(TASK_SLOT_COUNT > 0, "Zephyr task registry must have at least one slot");
static_assert(TASK_STACK_BYTES > 0, "Zephyr task stacks must not be empty");
static_assert(CONFIG_NUM_PREEMPT_PRIORITIES > 0, "Zephyr needs a preemptive priority");

struct TaskSlot
{
    k_thread thread{};
    TaskFunction function = nullptr;
    void *argument = nullptr;
    bool occupied = false;
};

TaskSlot s_taskSlots[TASK_SLOT_COUNT];
K_THREAD_STACK_ARRAY_DEFINE(s_taskStacks, TASK_SLOT_COUNT, TASK_STACK_BYTES);
k_spinlock s_taskTableLock;

TaskSlot *reserveTaskSlot()
{
    const auto key = k_spin_lock(&s_taskTableLock);
    TaskSlot *available = nullptr;
    for (auto &slot : s_taskSlots)
    {
        if (!slot.occupied)
        {
            slot.occupied = true;
            available = &slot;
            break;
        }
    }
    k_spin_unlock(&s_taskTableLock, key);
    return available;
}

void releaseTaskSlot(TaskSlot &slot)
{
    const auto key = k_spin_lock(&s_taskTableLock);
    slot.function = nullptr;
    slot.argument = nullptr;
    slot.occupied = false;
    k_spin_unlock(&s_taskTableLock, key);
}

TaskSlot *findTaskSlot(k_tid_t thread)
{
    const auto key = k_spin_lock(&s_taskTableLock);
    TaskSlot *found = nullptr;
    for (auto &slot : s_taskSlots)
    {
        if (slot.occupied && &slot.thread == thread)
        {
            found = &slot;
            break;
        }
    }
    k_spin_unlock(&s_taskTableLock, key);
    return found;
}

void taskEntry(void *slotPointer, void *, void *)
{
    auto &slot = *static_cast<TaskSlot *>(slotPointer);
    slot.function(slot.argument);
    releaseTaskSlot(slot);
}

bool translatePriority(std::uint32_t freeRtosPriority, int &zephyrPriority)
{
    if (freeRtosPriority > PRIO_CEIL)
        return false;

    // FreeRTOS priority N grows more urgent; Zephyr preemptive priorities grow
    // less urgent. Map the supported 0..PRIO_CEIL range to
    // K_PRIO_PREEMPT(PRIO_CEIL - N), preserving relative urgency.
    zephyrPriority = K_PRIO_PREEMPT(static_cast<int>(PRIO_CEIL - freeRtosPriority));
    return true;
}

k_timeout_t queueTimeout(std::uint32_t timeoutMilliseconds)
{
    const auto waitForever = static_cast<std::uint32_t>(
        rtos::backend::WAIT_FOREVER.count());
    return timeoutMilliseconds == waitForever
        ? K_FOREVER
        : K_MSEC(timeoutMilliseconds);
}

k_msgq *nativeQueue(rtos::backend::QueueHandle handle)
{
    return static_cast<k_msgq *>(handle);
}
}

namespace rtos::backend
{
bool task_create(
    TaskHandle &outHandle,
    const char *name,
    std::uint32_t stackSizeBytes,
    std::uint32_t priority,
    TaskFunction function,
    void *argument) noexcept
{
    outHandle = nullptr;
    int zephyrPriority = 0;
    if (!function || stackSizeBytes == 0 || stackSizeBytes > TASK_STACK_BYTES)
        return false;

    if (!translatePriority(priority, zephyrPriority))
    {
        RTOS_LOGE(
            "rtos.task",
            "task '%s' priority %u exceeds Zephyr preemptive ceiling %u",
            name ? name : "<unnamed>", priority, PRIO_CEIL);
        return false;
    }

    auto *slot = reserveTaskSlot();
    if (!slot)
        return false;

    slot->function = function;
    slot->argument = argument;
    const auto slotIndex = static_cast<std::size_t>(slot - s_taskSlots);
    auto *thread = k_thread_create(
        &slot->thread,
        s_taskStacks[slotIndex],
        K_THREAD_STACK_SIZEOF(s_taskStacks[slotIndex]),
        taskEntry,
        slot,
        nullptr,
        nullptr,
        zephyrPriority,
        0,
        K_NO_WAIT);
    if (!thread)
    {
        releaseTaskSlot(*slot);
        return false;
    }

    if (name)
        (void)k_thread_name_set(thread, name);
    outHandle = static_cast<TaskHandle>(thread);
    return true;
}

bool task_create_pinned(
    TaskHandle &outHandle,
    const char *name,
    std::uint32_t stackSizeBytes,
    std::uint32_t priority,
    int coreId,
    TaskFunction function,
    void *argument) noexcept
{
    // The nRF5340 application image runs on one application core, so a
    // FreeRTOS core-affinity request has no Zephyr equivalent and is ignored.
    (void)coreId;
    return task_create(
        outHandle, name, stackSizeBytes, priority, function, argument);
}

void task_delete(TaskHandle handle) noexcept
{
    if (!handle)
        return;

    auto *thread = static_cast<k_thread *>(handle);
    auto *slot = findTaskSlot(thread);
    if (!slot)
        return;

    if (thread == k_current_get())
    {
        releaseTaskSlot(*slot);
        k_thread_abort(thread);
        return;
    }

    k_thread_abort(thread);
    releaseTaskSlot(*slot);
}

void delay_ms(Millis duration) noexcept
{
    (void)k_sleep(K_MSEC(duration.count()));
}

void yield() noexcept
{
    k_yield();
}

TaskHandle current_task() noexcept
{
    return static_cast<TaskHandle>(k_current_get());
}

bool queue_create(
    QueueHandle &outHandle,
    std::size_t length,
    std::size_t itemSize) noexcept
{
    outHandle = nullptr;
    if (length == 0 || itemSize == 0 ||
        length > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    auto *queue = static_cast<k_msgq *>(k_malloc(sizeof(k_msgq)));
    if (!queue)
        return false;

    if (k_msgq_alloc_init(queue, itemSize, static_cast<std::uint32_t>(length)) != 0)
    {
        k_free(queue);
        return false;
    }

    outHandle = static_cast<QueueHandle>(queue);
    return true;
}

void queue_delete(QueueHandle handle) noexcept
{
    auto *queue = nativeQueue(handle);
    if (!queue)
        return;

    k_msgq_purge(queue);
    (void)k_msgq_cleanup(queue);
    k_free(queue);
}

bool queue_send(
    QueueHandle handle,
    const void *item,
    std::uint32_t timeoutMilliseconds) noexcept
{
    auto *queue = nativeQueue(handle);
    return queue && item &&
        k_msgq_put(queue, item, queueTimeout(timeoutMilliseconds)) == 0;
}

bool queue_receive(
    QueueHandle handle,
    void *outItem,
    std::uint32_t timeoutMilliseconds) noexcept
{
    auto *queue = nativeQueue(handle);
    return queue && outItem &&
        k_msgq_get(queue, outItem, queueTimeout(timeoutMilliseconds)) == 0;
}

bool queue_send_isr(
    QueueHandle handle,
    const void *item,
    bool *higherPriorityTaskWoken) noexcept
{
    if (higherPriorityTaskWoken)
        *higherPriorityTaskWoken = false;
    auto *queue = nativeQueue(handle);
    return queue && item && k_msgq_put(queue, item, K_NO_WAIT) == 0;
}

bool queue_receive_isr(
    QueueHandle handle,
    void *outItem,
    bool *higherPriorityTaskWoken) noexcept
{
    if (higherPriorityTaskWoken)
        *higherPriorityTaskWoken = false;
    auto *queue = nativeQueue(handle);
    return queue && outItem && k_msgq_get(queue, outItem, K_NO_WAIT) == 0;
}

std::size_t queue_count(QueueHandle handle) noexcept
{
    auto *queue = nativeQueue(handle);
    return queue ? k_msgq_num_used_get(queue) : 0;
}

std::size_t queue_spaces(QueueHandle handle) noexcept
{
    auto *queue = nativeQueue(handle);
    return queue ? k_msgq_num_free_get(queue) : 0;
}

bool queue_reset(QueueHandle handle) noexcept
{
    auto *queue = nativeQueue(handle);
    if (!queue)
        return false;
    k_msgq_purge(queue);
    return true;
}
}
