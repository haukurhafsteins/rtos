#pragma once

// Host stand-in for ESP-IDF's esp_log.h, shaped like v5.5 (components/log/include/esp_log.h and
// esp_log_level.h) as far as EspIdfLogSink uses it: the level enum, LOG_LOCAL_LEVEL falling back
// to CONFIG_LOG_MAXIMUM_LEVEL, and ESP_LOG_LEVEL_LOCAL gating on it before the call into the log
// library. The library call is replaced by a capture the test inspects.
#include <cstdarg>
#include <cstddef>

typedef enum
{
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR = 1,
    ESP_LOG_WARN = 2,
    ESP_LOG_INFO = 3,
    ESP_LOG_DEBUG = 4,
    ESP_LOG_VERBOSE = 5,
} esp_log_level_t;

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL 3 // Info, the meter's build (maximum follows the default level)
#endif

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL CONFIG_LOG_MAXIMUM_LEVEL
#endif

#define ESP_LOG_LEVEL_MASK 0x07
#define ESP_LOG_GET_LEVEL(config) ((config) & ESP_LOG_LEVEL_MASK)
#define ESP_LOG_ENABLED(configs) (LOG_LOCAL_LEVEL >= ESP_LOG_GET_LEVEL(configs))

// What the real macro hands to esp_log() after the LOG_LOCAL_LEVEL gate. The per-tag runtime
// level check that esp_log_va() performs is the library's, not the sink's, and is not modelled.
void esp_log_stub_capture(esp_log_level_t level, const char *tag, const char *format, ...);

#define ESP_LOG_LEVEL_LOCAL(configs, tag, format, ...)                                           \
    do                                                                                           \
    {                                                                                            \
        if (ESP_LOG_ENABLED(configs))                                                            \
        {                                                                                        \
            esp_log_stub_capture(static_cast<esp_log_level_t>(ESP_LOG_GET_LEVEL(configs)), tag, \
                                 format, __VA_ARGS__);                                           \
        }                                                                                        \
    } while (0)
