#include "rtos/FinalLogOutput.hpp"

#include <atomic>

#include <zephyr/kernel.h>

namespace
{
std::atomic<rtos::IFinalLogSink *> s_sink{nullptr};
}

namespace rtos
{
void setFinalLogSink(IFinalLogSink *sink) noexcept
{
    s_sink.store(sink, std::memory_order_release);
}

void clearFinalLogSink() noexcept
{
    s_sink.store(nullptr, std::memory_order_release);
}

void detail::dispatchFinalLogLine(
    const char *text, std::size_t length, bool truncated) noexcept
{
    // Native LOG/printk and interrupt output keep their console path; only
    // task-context RTOS formatter records enter the device-log sink.
    if (k_is_in_isr() || text == nullptr || length == 0)
        return;
    auto *sink = s_sink.load(std::memory_order_acquire);
    if (!sink)
        return;
    while (length != 0 && (text[length - 1] == '\r' || text[length - 1] == '\n'))
        --length;
    if (length != 0)
        sink->write(text, length, truncated);
}
}
