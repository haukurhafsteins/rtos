#include "rtos/Queue.hpp"
#include "rtos/MsgBuffer.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
constexpr uint32_t forever = std::numeric_limits<uint32_t>::max();
uint32_t queue_timeout = 0;
rtos::Millis message_timeout{0};
int queue_storage;
int message_storage;

void check(bool condition)
{
    if (!condition) {
        std::fputs("Timeout argument contract failed\n", stderr);
        std::abort();
    }
}
} // namespace

// Record the public wrappers' backend arguments without waiting or starting an RTOS.
namespace rtos::backend {
bool queue_create(QueueHandle& out, std::size_t, std::size_t) noexcept
{
    out = &queue_storage;
    return true;
}
void queue_delete(QueueHandle) noexcept {}
bool queue_send(QueueHandle, const void*, uint32_t timeout_ms) noexcept
{
    queue_timeout = timeout_ms;
    return true;
}
bool queue_receive(QueueHandle, void*, uint32_t timeout_ms) noexcept
{
    queue_timeout = timeout_ms;
    return true;
}
bool msgbuf_create(MsgBufferHandle& out, std::size_t) noexcept
{
    out = &message_storage;
    return true;
}
void msgbuf_delete(MsgBufferHandle) noexcept {}
std::size_t msgbuf_send(MsgBufferHandle, const void*, std::size_t bytes,
                        Millis timeout_ms) noexcept
{
    message_timeout = timeout_ms;
    return bytes;
}
std::size_t msgbuf_receive(MsgBufferHandle, void*, std::size_t bytes,
                           Millis timeout_ms) noexcept
{
    message_timeout = timeout_ms;
    return bytes;
}
} // namespace rtos::backend

int main()
{
    rtos::Queue<int> queue(1);
    rtos::MsgBuffer messages(64);
    int value = 42;

    check(queue.receive(value));
    check(queue_timeout == forever);
    check(queue.send(value));
    check(queue_timeout == 0);
    check(messages.send_obj(value));
    check(message_timeout == rtos::Millis::max());
    message_timeout = rtos::Millis{0};
    check(messages.receive_obj(value));
    check(message_timeout == rtos::Millis::max());

    // Include the largest finite value to guard the boundary beside the sentinel.
    const uint32_t timeouts[] = {0, 37, forever - 1, forever};
    for (const auto timeout : timeouts) {
        check(queue.receive(value, timeout));
        check(queue_timeout == timeout);
        queue_timeout = timeout + 1;
        check(queue.send(value, timeout));
        check(queue_timeout == timeout);

        const auto expected = timeout == forever ? rtos::Millis::max()
                                                 : rtos::Millis{timeout};
        message_timeout = rtos::Millis{-1};
        check(messages.send_obj(value, timeout));
        check(message_timeout == expected);
        message_timeout = rtos::Millis{-1};
        check(messages.receive_obj(value, timeout));
        check(message_timeout == expected);
    }

    check(queue.try_send(value));
    check(queue_timeout == 0);
    queue_timeout = forever;
    check(queue.try_receive(value));
    check(queue_timeout == 0);

    // The raw chrono API must still distinguish finite UINT32_MAX from forever.
    const rtos::Millis durations[] = {rtos::Millis{0}, rtos::Millis{37},
                                     rtos::Millis{forever}, rtos::Millis::max()};
    for (const auto duration : durations) {
        message_timeout = rtos::Millis{-1};
        check(messages.send_all(&value, sizeof(value), duration));
        check(message_timeout == duration);
        message_timeout = rtos::Millis{-1};
        check(messages.receive(&value, sizeof(value), duration) == sizeof(value));
        check(message_timeout == duration);
    }
}
