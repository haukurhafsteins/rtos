#pragma once

#include <cstddef>

namespace rtos
{
    /**
     * Receives formatted text from the platform's supported capture path.
     *
     * The sink is externally owned and must remain alive until it is cleared.
     * write() can be called concurrently from multiple tasks. It must not block,
     * allocate, or log. Normal logging and this callback are not ISR-safe.
     */
    struct IFinalLogSink
    {
        virtual ~IFinalLogSink() = default;
        virtual void write(
            const char *text,
            std::size_t len,
            bool truncated) noexcept = 0;
    };

    /**
     * Installs or replaces the final-output sink.
     *
     * On ESP-IDF, the first call installs a vprintf tee and preserves the
     * previously configured output handler. On Zephyr, only task-context
     * RTOS-originated messages are captured at the formatter boundary, once per
     * log call. Native LOG/printk and ISR output remain console-only. Trailing
     * CR/LF is removed from captured records; embedded newlines are retained.
     * Formatter overflow is reported as truncation. Console output is unchanged.
     * Registration is startup-only.
     */
    void setFinalLogSink(IFinalLogSink *sink) noexcept;

    /**
     * Stops final-output delivery without changing the platform output hook.
     */
    void clearFinalLogSink() noexcept;

#if defined(__ZEPHYR__)
    namespace detail
    {
        // Internal formatter seam: caller holds its existing recursion guard
        // and owns text until this synchronous, allocation-free call returns.
        void dispatchFinalLogLine(
            const char *text, std::size_t length, bool truncated) noexcept;
    }
#endif
}
