#pragma once

#include <cstdint>
#include <cstdarg>
#include <stddef.h>

// ---------- Build-time configuration (override in your project) ----------
#ifndef RTOS_LOG_MAX_SINKS
#define RTOS_LOG_MAX_SINKS 4
#endif

#ifndef RTOS_LOG_MAX_TAG_RULES
#define RTOS_LOG_MAX_TAG_RULES 16
#endif

#ifndef RTOS_LOG_LINE_MAX
#define RTOS_LOG_LINE_MAX 256
#endif

#ifndef RTOS_LOG_SHOW_TIME
#define RTOS_LOG_SHOW_TIME 1
#endif

#ifndef RTOS_LOG_SHOW_FILELINE
#define RTOS_LOG_SHOW_FILELINE 1
#endif

// Integer constants for preprocessing comparisons
#define RTOS_LOG_LEVEL_NONE    0
#define RTOS_LOG_LEVEL_ERROR   1
#define RTOS_LOG_LEVEL_WARN    2
#define RTOS_LOG_LEVEL_INFO    3
#define RTOS_LOG_LEVEL_DEBUG   4
#define RTOS_LOG_LEVEL_VERBOSE 5

#ifndef RTOS_LOG_BUILD_LEVEL
#define RTOS_LOG_BUILD_LEVEL RTOS_LOG_LEVEL_INFO
#endif

// Per-file convenience: define RTOS_LOG_TAG before including this header to avoid
// passing the tag for every call.
#ifndef RTOS_LOG_TAG
#define RTOS_LOG_TAG nullptr
#endif

namespace rtos
{
    enum class LogLevel : uint8_t
    {
        None    = RTOS_LOG_LEVEL_NONE,
        Error   = RTOS_LOG_LEVEL_ERROR,
        Warn    = RTOS_LOG_LEVEL_WARN,
        Info    = RTOS_LOG_LEVEL_INFO,
        Debug   = RTOS_LOG_LEVEL_DEBUG,
        Verbose = RTOS_LOG_LEVEL_VERBOSE,
    };

    struct IRtosLogSink
    {
        virtual ~IRtosLogSink() = default;
        virtual bool enabled(LogLevel) { return true; }
        virtual void write(LogLevel level, const char* tag, const char* line, size_t len) = 0;
    };

    class RtosLog
    {
    public:
        using TimestampFn = uint32_t (*)();

        static void addSink(IRtosLogSink& sink);
        static void clearSinks();

        static void setGlobalLevel(LogLevel lvl);
        static LogLevel getGlobalLevel();

        static void setTagLevel(const char* tag, LogLevel lvl);  // keeps pointer, recommend static string literal
        static LogLevel getTagLevel(const char* tag);            // returns effective or None if not set

        static void setTimestampProvider(TimestampFn fn);

        static void vlog(LogLevel level, const char* tag, const char* fmt, va_list ap);
        static void log(LogLevel level, const char* tag, const char* fmt, ...);

        // Helpers for converting, exposed for sinks
        static char levelChar(LogLevel lvl);

    private:
        static bool shouldEmit(LogLevel level, const char* tag);
        static void lock();
        static void unlock();
    };
}

// ---------- User-facing macros (compile-time filtered) ----------

#if RTOS_LOG_BUILD_LEVEL >= RTOS_LOG_LEVEL_ERROR
#define RTOS_LOGE(TAG, FMT, ...) ::rtos::RtosLog::log(::rtos::LogLevel::Error,  TAG, FMT, ##__VA_ARGS__)
#else
#define RTOS_LOGE(...) do{}while(0)
#endif

#if RTOS_LOG_BUILD_LEVEL >= RTOS_LOG_LEVEL_WARN
#define RTOS_LOGW(TAG, FMT, ...) ::rtos::RtosLog::log(::rtos::LogLevel::Warn,   TAG, FMT, ##__VA_ARGS__)
#else
#define RTOS_LOGW(...) do{}while(0)
#endif

#if RTOS_LOG_BUILD_LEVEL >= RTOS_LOG_LEVEL_INFO
#define RTOS_LOGI(TAG, FMT, ...) ::rtos::RtosLog::log(::rtos::LogLevel::Info,   TAG, FMT, ##__VA_ARGS__)
#else
#define RTOS_LOGI(...) do{}while(0)
#endif

#if RTOS_LOG_BUILD_LEVEL >= RTOS_LOG_LEVEL_DEBUG
#define RTOS_LOGD(TAG, FMT, ...) ::rtos::RtosLog::log(::rtos::LogLevel::Debug,  TAG, FMT, ##__VA_ARGS__)
#else
#define RTOS_LOGD(...) do{}while(0)
#endif

#if RTOS_LOG_BUILD_LEVEL >= RTOS_LOG_LEVEL_VERBOSE
#define RTOS_LOGV(TAG, FMT, ...) ::rtos::RtosLog::log(::rtos::LogLevel::Verbose,TAG, FMT, ##__VA_ARGS__)
#else
#define RTOS_LOGV(...) do{}while(0)
#endif

// Per-file tag variants
#define LOGE(FMT, ...) RTOS_LOGE(RTOS_LOG_TAG, FMT, ##__VA_ARGS__)
#define LOGW(FMT, ...) RTOS_LOGW(RTOS_LOG_TAG, FMT, ##__VA_ARGS__)
#define LOGI(FMT, ...) RTOS_LOGI(RTOS_LOG_TAG, FMT, ##__VA_ARGS__)
#define LOGD(FMT, ...) RTOS_LOGD(RTOS_LOG_TAG, FMT, ##__VA_ARGS__)
#define LOGV(FMT, ...) RTOS_LOGV(RTOS_LOG_TAG, FMT, ##__VA_ARGS__)
