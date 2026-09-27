#pragma once
#include "rtos/Log.hpp"

namespace rtos
{
    // A minimal portable sink that prints the line verbatim to a C stdio stream.
    class StdoutLogSink final : public ILogSink
    {
    public:
        explicit StdoutLogSink(void* stream = nullptr) : _stream(stream) {}
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    private:
        void* _stream; // FILE* but kept as void* to avoid <cstdio> in header
    };

    // For ESP-IDF (FreeRTOS on ESP32). Prints the native "L (ms) tag: body" line through the
    // same macro path as ESP_LOGx, once, with ESP-IDF's compile-time and per-tag levels applied.
    // On espidf the backend emits directly only while no sink is registered, so registering this
    // sink replaces, not duplicates, the console output (HMO-86).
    class EspIdfLogSink final : public ILogSink
    {
    public:
        void writeRecord(const LogRecord& record) override;
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    };

    // For Zephyr: uses printk().
    class ZephyrPrintkSink final : public ILogSink
    {
    public:
        void write(LogLevel level, const char* tag, const char* line, size_t len) override;
    };
}
