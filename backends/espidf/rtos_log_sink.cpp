#include "rtos/LogSinks.hpp"

#include "esp_log.h"

// The ESP-IDF sink prints the native "L (ms) tag: body" line and nothing else.
//
// It goes through ESP_LOG_LEVEL_LOCAL, the runtime-level macro every ESP_LOGx expands to, so the
// output is what ESP_LOGx would have printed, byte for byte: same LOG_FORMAT/colour/timestamp
// choice, same compile-time cap (LOG_LOCAL_LEVEL, i.e. CONFIG_LOG_MAXIMUM_LEVEL; Debug is
// dropped here exactly as ESP_LOGD is compiled out) and the same per-tag runtime level applied
// inside esp_log_va. A plain esp_log_write() would skip the cap and add no prefix, so writing
// the rtos "[ts] L/tag: body" line through it would neither filter nor look native (HMO-86).
//
// Kept in its own translation unit so the host tests can compile it against a stub esp_log.h.

namespace
{
    bool toEspLevel(rtos::LogLevel level, esp_log_level_t &out)
    {
        switch (level)
        {
        case rtos::LogLevel::Error:
            out = ESP_LOG_ERROR;
            return true;
        case rtos::LogLevel::Warn:
            out = ESP_LOG_WARN;
            return true;
        case rtos::LogLevel::Info:
            out = ESP_LOG_INFO;
            return true;
        case rtos::LogLevel::Debug:
            out = ESP_LOG_DEBUG;
            return true;
        case rtos::LogLevel::Verbose:
            out = ESP_LOG_VERBOSE;
            return true;
        default:
            return false;
        }
    }

    void emit(rtos::LogLevel level, const char *tag, const char *text)
    {
        esp_log_level_t espLevel;
        if (!toEspLevel(level, espLevel))
            return;
        ESP_LOG_LEVEL_LOCAL(espLevel, tag, "%s", text);
    }
}

namespace rtos
{
    void EspIdfLogSink::writeRecord(const LogRecord &record)
    {
        emit(record.level, record.tag, record.body);
    }

    // Only reached by a caller that hands over a pre-formatted line itself; the backend
    // always uses writeRecord(). The line is printed as the message, once.
    void EspIdfLogSink::write(LogLevel level, const char *tag, const char *line, size_t)
    {
        emit(level, tag, line);
    }
}
