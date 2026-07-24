#include "rtos/FinalLogOutput.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>

#include "esp_log.h"
#include "rtos/Log.hpp"

namespace
{
    std::atomic<rtos::IFinalLogSink *> s_sink{nullptr};
    vprintf_like_t s_priorOutput = nullptr;
    bool s_installed = false;
    thread_local bool s_inSinkDispatch = false;

    class SinkDispatchGuard
    {
    public:
        SinkDispatchGuard() { s_inSinkDispatch = true; }
        ~SinkDispatchGuard() { s_inSinkDispatch = false; }
    };

    int finalOutputTee(const char *format, va_list args)
    {
        va_list priorArgs;
        va_copy(priorArgs, args);
        const int result = s_priorOutput
            ? s_priorOutput(format, priorArgs)
            : std::vprintf(format, priorArgs);
        va_end(priorArgs);

        auto *sink = s_sink.load(std::memory_order_acquire);
        if (sink == nullptr || s_inSinkDispatch)
            return result;

        char text[RTOS_LOG_LINE_MAX];
        va_list sinkArgs;
        va_copy(sinkArgs, args);
        const int formatted = std::vsnprintf(
            text, sizeof(text), format ? format : "", sinkArgs);
        va_end(sinkArgs);
        if (formatted < 0)
            return result;

        const auto length =
            formatted < static_cast<int>(sizeof(text))
                ? static_cast<std::size_t>(formatted)
                : sizeof(text) - 1;
        const bool truncated = formatted >= static_cast<int>(sizeof(text));

        SinkDispatchGuard guard;
        sink->write(text, length, truncated);
        return result;
    }
}

namespace rtos
{
    void setFinalLogSink(IFinalLogSink *sink) noexcept
    {
        if (!s_installed)
        {
            s_priorOutput = esp_log_set_vprintf(finalOutputTee);
            s_installed = true;
        }
        s_sink.store(sink, std::memory_order_release);
    }

    void clearFinalLogSink() noexcept
    {
        s_sink.store(nullptr, std::memory_order_release);
    }
}
