#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "RtosLog.hpp"
#include "RtosLogSinks.hpp"
#include "rtos_psram.hpp"

namespace
{
struct TagRule
{
	const char *tag;
	rtos::LogLevel level;
};

rtos::IRtosLogSink *s_sinks[RTOS_LOG_MAX_SINKS] = {};
std::size_t s_sinkCount = 0;
rtos::LogLevel s_globalLevel = rtos::LogLevel::Info;
TagRule s_rules[RTOS_LOG_MAX_TAG_RULES] = {};
std::size_t s_ruleCount = 0;
rtos::RtosLog::TimestampFn s_timestampProvider = nullptr;
std::mutex s_logMutex;
}

namespace rtos {

void RtosLog::addSink(IRtosLogSink &sink)
{
	std::lock_guard<std::mutex> lock(s_logMutex);
	if (s_sinkCount < RTOS_LOG_MAX_SINKS)
		s_sinks[s_sinkCount++] = &sink;
}

void RtosLog::clearSinks()
{
	std::lock_guard<std::mutex> lock(s_logMutex);
	s_sinkCount = 0;
	std::memset(s_sinks, 0, sizeof(s_sinks));
}

void RtosLog::setGlobalLevel(LogLevel lvl) { s_globalLevel = lvl; }

LogLevel RtosLog::getGlobalLevel() { return s_globalLevel; }

void RtosLog::setTagLevel(const char *tag, LogLevel lvl)
{
	if (!tag)
		return;

	std::lock_guard<std::mutex> lock(s_logMutex);
	for (std::size_t i = 0; i < s_ruleCount; ++i)
	{
		if (s_rules[i].tag == tag || (s_rules[i].tag && std::strcmp(s_rules[i].tag, tag) == 0))
		{
			s_rules[i].level = lvl;
			return;
		}
	}

	if (s_ruleCount < RTOS_LOG_MAX_TAG_RULES)
		s_rules[s_ruleCount++] = TagRule{tag, lvl};
}

LogLevel RtosLog::getTagLevel(const char *tag)
{
	if (!tag)
		return LogLevel::None;

	std::lock_guard<std::mutex> lock(s_logMutex);
	for (std::size_t i = 0; i < s_ruleCount; ++i)
	{
		if (s_rules[i].tag == tag || (s_rules[i].tag && std::strcmp(s_rules[i].tag, tag) == 0))
			return s_rules[i].level;
	}

	return LogLevel::None;
}

void RtosLog::setTimestampProvider(TimestampFn fn) { s_timestampProvider = fn; }

char RtosLog::levelChar(LogLevel lvl)
{
	switch (lvl)
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

bool RtosLog::shouldEmit(LogLevel level, const char *tag)
{
	LogLevel tagLevel = getTagLevel(tag);
	LogLevel gate = (tagLevel != LogLevel::None) ? tagLevel : s_globalLevel;
	return static_cast<int>(level) <= static_cast<int>(gate);
}

void RtosLog::lock() { s_logMutex.lock(); }

void RtosLog::unlock() { s_logMutex.unlock(); }

void RtosLog::vlog(LogLevel level, const char *tag, const char *fmt, va_list ap)
{
	if (!shouldEmit(level, tag))
		return;

	char body[RTOS_LOG_LINE_MAX];
	va_list apCopy;
	va_copy(apCopy, ap);
	int n = std::vsnprintf(body, sizeof(body), fmt ? fmt : "", apCopy);
	va_end(apCopy);
	if (n < 0)
		return;

	const char *resolvedTag = tag ? tag : "rtos";
	char line[RTOS_LOG_LINE_MAX];
	int lineLength = 0;
#if RTOS_LOG_SHOW_TIME
	unsigned long timestamp = s_timestampProvider ? s_timestampProvider() : 0UL;
	lineLength = std::snprintf(line, sizeof(line), "[%lu] %c/%s: %s", timestamp, levelChar(level), resolvedTag, body);
#else
	lineLength = std::snprintf(line, sizeof(line), "%c/%s: %s", levelChar(level), resolvedTag, body);
#endif
	if (lineLength < 0)
		return;

	std::size_t emittedLength = (lineLength < static_cast<int>(sizeof(line)))
		? static_cast<std::size_t>(lineLength)
		: sizeof(line) - 1;

	std::lock_guard<std::mutex> lock(s_logMutex);
	for (std::size_t i = 0; i < s_sinkCount; ++i)
	{
		auto *sink = s_sinks[i];
		if (sink && sink->enabled(level))
			sink->write(level, resolvedTag, line, emittedLength);
	}
}

void RtosLog::log(LogLevel level, const char *tag, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vlog(level, tag, fmt, ap);
	va_end(ap);
}

void StdoutLogSink::write(LogLevel, const char *, const char *line, size_t)
{
	auto *stream = static_cast<FILE *>(_stream ? _stream : stdout);
	std::fputs(line, stream);
	std::fputc('\n', stream);
}

}

namespace rtos::memory {

void *rtos_psram_malloc(std::size_t size)
{
	void *ptr = std::malloc(size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *rtos_psram_calloc(std::size_t num, std::size_t size)
{
	void *ptr = std::calloc(1, num * size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = num * size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *rtos_psram_realloc(void *ptr, std::size_t size)
{
	if (!ptr)
		return rtos_psram_malloc(size);
	void *original = static_cast<char *>(ptr) - sizeof(std::size_t);
	void *resized = std::realloc(original, size + sizeof(std::size_t));
	if (!resized)
		return nullptr;
	*static_cast<std::size_t *>(resized) = size;
	return static_cast<char *>(resized) + sizeof(std::size_t);
}

void rtos_psram_free(void *ptr)
{
	if (!ptr)
		return;
	std::free(static_cast<char *>(ptr) - sizeof(std::size_t));
}

std::size_t rtos_psram_allocated_size(void *ptr)
{
	if (!ptr)
		return 0;
	return *reinterpret_cast<std::size_t *>(static_cast<char *>(ptr) - sizeof(std::size_t));
}

}