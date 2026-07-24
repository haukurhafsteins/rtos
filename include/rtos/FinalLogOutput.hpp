#pragma once

#include <cstddef>

namespace rtos
{
    /**
     * Receives the final text emitted through the platform logging backend.
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
     * previously configured output handler. Registration is startup-only.
     */
    void setFinalLogSink(IFinalLogSink *sink) noexcept;

    /**
     * Stops final-output delivery without changing the platform UART handler.
     */
    void clearFinalLogSink() noexcept;
}
