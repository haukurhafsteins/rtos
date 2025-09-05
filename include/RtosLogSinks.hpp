#pragma once
#include "RtosLog.hpp"

namespace rtos
{
    // A minimal portable sink that prints the line verbatim to a C stdio stream.
    class StdoutLogSink final : public IRtosLogSink
    {
    public:
        explicit StdoutLogSink(void* stream = nullptr) : _stream(stream) {}
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    private:
        void* _stream; // FILE* but kept as void* to avoid <cstdio> in header
    };

    // For ESP-IDF (FreeRTOS on ESP32). Compiles only if ESP_PLATFORM is defined.
    class EspIdfLogSink final : public IRtosLogSink
    {
    public:
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    };

    // For Zephyr: uses printk().
    class ZephyrPrintkSink final : public IRtosLogSink
    {
    public:
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    };
}
