#include "rtos/FinalLogOutput.hpp"

#include <atomic>

#include <zephyr/sys/printk-hooks.h>

namespace
{
std::atomic<rtos::IFinalLogSink *> s_sink{nullptr};
printk_hook_fn_t s_priorOutput = nullptr;
bool s_installed = false;
thread_local bool s_inSinkDispatch = false;

int finalOutputTee(int character)
{
    const int result = s_priorOutput ? s_priorOutput(character) : character;
    auto *sink = s_sink.load(std::memory_order_acquire);
    if (!sink || s_inSinkDispatch)
        return result;

    const char text = static_cast<char>(character);
    s_inSinkDispatch = true;
    sink->write(&text, 1, false);
    s_inSinkDispatch = false;
    return result;
}
}

namespace rtos
{
void setFinalLogSink(IFinalLogSink *sink) noexcept
{
    if (!s_installed)
    {
        s_priorOutput = __printk_get_hook();
        __printk_hook_install(finalOutputTee);
        s_installed = true;
    }
    s_sink.store(sink, std::memory_order_release);
}

void clearFinalLogSink() noexcept
{
    s_sink.store(nullptr, std::memory_order_release);
}
}
