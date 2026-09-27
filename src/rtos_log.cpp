#include "rtos/Log.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace
{
struct TagRule
{
    const char *tag;
    rtos::LogLevel level;
};

rtos::ILogSink *s_sinks[RTOS_LOG_MAX_SINKS] = {};
std::size_t s_sinkCount = 0;
rtos::LogLevel s_globalLevel = rtos::LogLevel::Info;
TagRule s_rules[RTOS_LOG_MAX_TAG_RULES] = {};
std::size_t s_ruleCount = 0;
rtos::Log::TimestampFn s_timestampProvider = nullptr;
thread_local bool s_inSinkDispatch = false;

class SinkDispatchGuard
{
public:
    SinkDispatchGuard() { s_inSinkDispatch = true; }
    ~SinkDispatchGuard() { s_inSinkDispatch = false; }
};
}

namespace rtos
{
void Log::addSink(ILogSink &sink)
{
    lock();
    if (s_sinkCount < RTOS_LOG_MAX_SINKS)
        s_sinks[s_sinkCount++] = &sink;
    unlock();
}

void Log::clearSinks()
{
    lock();
    s_sinkCount = 0;
    std::memset(s_sinks, 0, sizeof(s_sinks));
    unlock();
}

void Log::setGlobalLevel(LogLevel level)
{
    s_globalLevel = level;
}

LogLevel Log::getGlobalLevel()
{
    return s_globalLevel;
}

void Log::setTagLevel(const char *tag, LogLevel level)
{
    if (!tag)
        return;

    lock();
    for (std::size_t index = 0; index < s_ruleCount; ++index)
    {
        if (s_rules[index].tag == tag ||
            (s_rules[index].tag && std::strcmp(s_rules[index].tag, tag) == 0))
        {
            s_rules[index].level = level;
            unlock();
            return;
        }
    }

    if (s_ruleCount < RTOS_LOG_MAX_TAG_RULES)
        s_rules[s_ruleCount++] = TagRule{tag, level};
    unlock();
}

LogLevel Log::getTagLevel(const char *tag)
{
    if (!tag)
        return LogLevel::None;

    lock();
    for (std::size_t index = 0; index < s_ruleCount; ++index)
    {
        if (s_rules[index].tag == tag ||
            (s_rules[index].tag && std::strcmp(s_rules[index].tag, tag) == 0))
        {
            const auto level = s_rules[index].level;
            unlock();
            return level;
        }
    }
    unlock();
    return LogLevel::None;
}

void Log::setTimestampProvider(TimestampFn provider)
{
    s_timestampProvider = provider;
}

char Log::levelChar(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Error:
        return 'E';
    case LogLevel::Warn:
        return 'W';
    case LogLevel::Info:
        return 'I';
    case LogLevel::Debug:
        return 'D';
    case LogLevel::Verbose:
        return 'V';
    default:
        return '-';
    }
}

bool Log::shouldEmit(LogLevel level, const char *tag)
{
    const auto tagLevel = getTagLevel(tag);
    const auto gate = tagLevel != LogLevel::None ? tagLevel : s_globalLevel;
    return static_cast<int>(level) <= static_cast<int>(gate);
}

void Log::vlog(LogLevel level, const char *tag, const char *format, va_list args)
{
    if (!shouldEmit(level, tag))
        return;

    char body[RTOS_LOG_LINE_MAX];
    va_list copy;
    va_copy(copy, args);
    const int formatted = std::vsnprintf(body, sizeof(body), format ? format : "", copy);
    va_end(copy);
    if (formatted < 0)
        return;

    const char *resolvedTag = tag ? tag : "rtos";
    char line[RTOS_LOG_LINE_MAX];
#if RTOS_LOG_SHOW_TIME
    const auto timestamp = s_timestampProvider ? s_timestampProvider() : 0U;
    const int lineLength = std::snprintf(
        line, sizeof(line), "[%lu] %c/%s: %s",
        static_cast<unsigned long>(timestamp), levelChar(level), resolvedTag, body);
#else
    const int lineLength = std::snprintf(
        line, sizeof(line), "%c/%s: %s", levelChar(level), resolvedTag, body);
#endif
    if (lineLength < 0 || s_inSinkDispatch)
        return;

    const auto emittedLength = lineLength < static_cast<int>(sizeof(line))
        ? static_cast<std::size_t>(lineLength)
        : sizeof(line) - 1;
    const auto bodyLength = formatted < static_cast<int>(sizeof(body))
        ? static_cast<std::size_t>(formatted)
        : sizeof(body) - 1;

    std::array<ILogSink *, RTOS_LOG_MAX_SINKS> sinks{};
    lock();
    const auto sinkCount = s_sinkCount;
    std::copy_n(s_sinks, sinkCount, sinks.begin());
    unlock();

    const LogRecord record{level, resolvedTag, body, bodyLength, line, emittedLength};

    SinkDispatchGuard guard;
    for (std::size_t index = 0; index < sinkCount; ++index)
    {
        auto *sink = sinks[index];
        if (sink && sink->enabled(level))
            sink->writeRecord(record);
    }
}

void Log::log(LogLevel level, const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vlog(level, tag, format, args);
    va_end(args);
}
}
