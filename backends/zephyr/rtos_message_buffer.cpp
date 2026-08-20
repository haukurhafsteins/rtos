#include "rtos/backend.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

#include <zephyr/kernel.h>

namespace
{
constexpr std::size_t LENGTH_PREFIX_BYTES = sizeof(std::uint16_t);

struct MessageBufferState
{
    k_spinlock lock{};
    k_sem dataAvailable{};
    k_sem spaceAvailable{};
    std::uint8_t *storage = nullptr;
    std::size_t capacity = 0;
    std::size_t used = 0;
    std::size_t readIndex = 0;
    std::size_t writeIndex = 0;
};

class WaitBudget
{
public:
    explicit WaitBudget(rtos::Millis timeout) noexcept
        : forever_(timeout == rtos::Millis::max())
    {
        if (forever_)
            return;

        const auto duration = std::max<std::int64_t>(0, timeout.count());
        const auto now = k_uptime_get();
        deadline_ = duration > std::numeric_limits<std::int64_t>::max() - now
            ? std::numeric_limits<std::int64_t>::max()
            : now + duration;
    }

    bool expired() const noexcept
    {
        return !forever_ && k_uptime_get() >= deadline_;
    }

    k_timeout_t remaining() const noexcept
    {
        if (forever_)
            return K_FOREVER;
        return K_MSEC(std::max<std::int64_t>(0, deadline_ - k_uptime_get()));
    }

private:
    bool forever_ = false;
    std::int64_t deadline_ = 0;
};

enum class ReceiveStatus
{
    Empty,
    OutputTooSmall,
    Received,
};

struct ReceiveResult
{
    ReceiveStatus status = ReceiveStatus::Empty;
    std::size_t bytes = 0;
};

MessageBufferState *nativeBuffer(rtos::backend::MsgBufferHandle handle)
{
    return static_cast<MessageBufferState *>(handle);
}

void ringWrite(
    MessageBufferState &state, const std::uint8_t *source, std::size_t bytes)
{
    const auto first = std::min(bytes, state.capacity - state.writeIndex);
    std::memcpy(state.storage + state.writeIndex, source, first);
    std::memcpy(state.storage, source + first, bytes - first);
    state.writeIndex = (state.writeIndex + bytes) % state.capacity;
}

void ringRead(
    MessageBufferState &state, std::uint8_t *destination, std::size_t bytes)
{
    const auto first = std::min(bytes, state.capacity - state.readIndex);
    std::memcpy(destination, state.storage + state.readIndex, first);
    std::memcpy(destination + first, state.storage, bytes - first);
    state.readIndex = (state.readIndex + bytes) % state.capacity;
}

std::uint16_t peekLength(const MessageBufferState &state)
{
    const auto low = state.storage[state.readIndex];
    const auto high = state.storage[(state.readIndex + 1U) % state.capacity];
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        (static_cast<std::uint16_t>(high) << 8U));
}

bool trySend(MessageBufferState &state, const void *data, std::size_t bytes)
{
    const auto frameBytes = LENGTH_PREFIX_BYTES + bytes;
    const auto key = k_spin_lock(&state.lock);
    if (frameBytes > state.capacity - state.used)
    {
        k_spin_unlock(&state.lock, key);
        return false;
    }

    const std::uint8_t length[] = {
        static_cast<std::uint8_t>(bytes & 0xffU),
        static_cast<std::uint8_t>((bytes >> 8U) & 0xffU),
    };
    ringWrite(state, length, sizeof(length));
    ringWrite(state, static_cast<const std::uint8_t *>(data), bytes);
    state.used += frameBytes;
    k_spin_unlock(&state.lock, key);
    k_sem_give(&state.dataAvailable);
    return true;
}

ReceiveResult tryReceive(
    MessageBufferState &state, void *output, std::size_t maxBytes)
{
    const auto key = k_spin_lock(&state.lock);
    if (state.used < LENGTH_PREFIX_BYTES)
    {
        k_spin_unlock(&state.lock, key);
        return {};
    }

    const auto messageBytes = static_cast<std::size_t>(peekLength(state));
    if (messageBytes > maxBytes)
    {
        k_spin_unlock(&state.lock, key);
        return {ReceiveStatus::OutputTooSmall, 0};
    }

    std::uint8_t ignoredLength[LENGTH_PREFIX_BYTES]{};
    ringRead(state, ignoredLength, sizeof(ignoredLength));
    ringRead(state, static_cast<std::uint8_t *>(output), messageBytes);
    state.used -= LENGTH_PREFIX_BYTES + messageBytes;
    k_spin_unlock(&state.lock, key);
    k_sem_give(&state.spaceAvailable);
    return {ReceiveStatus::Received, messageBytes};
}

bool validPayload(const void *data, std::size_t bytes)
{
    return data && bytes > 0 &&
        bytes <= std::numeric_limits<std::uint16_t>::max();
}
}

namespace rtos::backend
{
bool msgbuf_create(
    MsgBufferHandle &outHandle, std::size_t capacityBytes) noexcept
{
    outHandle = nullptr;
    if (capacityBytes <= LENGTH_PREFIX_BYTES)
        return false;

    auto *memory = k_malloc(sizeof(MessageBufferState));
    if (!memory)
        return false;
    auto *state = new (memory) MessageBufferState{};

    state->storage = static_cast<std::uint8_t *>(k_malloc(capacityBytes));
    if (!state->storage)
    {
        state->~MessageBufferState();
        k_free(state);
        return false;
    }

    state->capacity = capacityBytes;
    if (k_sem_init(&state->dataAvailable, 0, 1) != 0 ||
        k_sem_init(&state->spaceAvailable, 0, 1) != 0)
    {
        k_free(state->storage);
        state->~MessageBufferState();
        k_free(state);
        return false;
    }

    outHandle = static_cast<MsgBufferHandle>(state);
    return true;
}

void msgbuf_delete(MsgBufferHandle handle) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state)
        return;

    k_free(state->storage);
    state->~MessageBufferState();
    k_free(state);
}

std::size_t msgbuf_send(
    MsgBufferHandle handle,
    const void *data,
    std::size_t bytes,
    Millis timeout) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state || !validPayload(data, bytes) ||
        LENGTH_PREFIX_BYTES + bytes > state->capacity)
    {
        return 0;
    }

    WaitBudget budget(timeout);
    while (!trySend(*state, data, bytes))
    {
        if (budget.expired() ||
            k_sem_take(&state->spaceAvailable, budget.remaining()) != 0)
        {
            return 0;
        }
    }
    return bytes;
}

std::size_t msgbuf_receive(
    MsgBufferHandle handle,
    void *output,
    std::size_t maxBytes,
    Millis timeout) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state || !output || maxBytes == 0)
        return 0;

    WaitBudget budget(timeout);
    while (true)
    {
        const auto result = tryReceive(*state, output, maxBytes);
        if (result.status == ReceiveStatus::Received)
            return result.bytes;
        if (result.status == ReceiveStatus::OutputTooSmall || budget.expired())
            return 0;
        if (k_sem_take(&state->dataAvailable, budget.remaining()) != 0)
            return 0;
    }
}

std::size_t msgbuf_send_isr(
    MsgBufferHandle handle,
    const void *data,
    std::size_t bytes,
    bool *higherPriorityTaskWoken) noexcept
{
    if (higherPriorityTaskWoken)
        *higherPriorityTaskWoken = false;
    auto *state = nativeBuffer(handle);
    if (!state || !validPayload(data, bytes) ||
        LENGTH_PREFIX_BYTES + bytes > state->capacity)
    {
        return 0;
    }
    return trySend(*state, data, bytes) ? bytes : 0;
}

std::size_t msgbuf_receive_isr(
    MsgBufferHandle handle,
    void *output,
    std::size_t maxBytes,
    bool *higherPriorityTaskWoken) noexcept
{
    if (higherPriorityTaskWoken)
        *higherPriorityTaskWoken = false;
    auto *state = nativeBuffer(handle);
    if (!state || !output || maxBytes == 0)
        return 0;
    const auto result = tryReceive(*state, output, maxBytes);
    return result.status == ReceiveStatus::Received ? result.bytes : 0;
}

std::size_t msgbuf_next_len(MsgBufferHandle handle) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state)
        return 0;

    const auto key = k_spin_lock(&state->lock);
    const auto bytes = state->used >= LENGTH_PREFIX_BYTES
        ? static_cast<std::size_t>(peekLength(*state))
        : 0;
    k_spin_unlock(&state->lock, key);
    return bytes;
}

std::size_t msgbuf_space_available(MsgBufferHandle handle) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state)
        return 0;

    const auto key = k_spin_lock(&state->lock);
    const auto bytes = state->capacity - state->used;
    k_spin_unlock(&state->lock, key);
    return bytes;
}

bool msgbuf_reset(MsgBufferHandle handle) noexcept
{
    auto *state = nativeBuffer(handle);
    if (!state)
        return false;

    const auto key = k_spin_lock(&state->lock);
    state->used = 0;
    state->readIndex = 0;
    state->writeIndex = 0;
    k_spin_unlock(&state->lock, key);
    k_sem_reset(&state->dataAvailable);
    k_sem_reset(&state->spaceAvailable);
    k_sem_give(&state->spaceAvailable);
    return true;
}
}
