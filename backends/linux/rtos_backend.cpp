#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "rtos/AppInfo.hpp"
#include "rtos/backend.hpp"
#include "rtos/Log.hpp"
#include "rtos/LogSinks.hpp"
#include "rtos/psram.hpp"

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
std::mutex s_logMutex;
}

namespace rtos {

void Log::addSink(ILogSink &sink)
{
	std::lock_guard<std::mutex> lock(s_logMutex);
	if (s_sinkCount < RTOS_LOG_MAX_SINKS)
		s_sinks[s_sinkCount++] = &sink;
}

void Log::clearSinks()
{
	std::lock_guard<std::mutex> lock(s_logMutex);
	s_sinkCount = 0;
	std::memset(s_sinks, 0, sizeof(s_sinks));
}

void Log::setGlobalLevel(LogLevel lvl) { s_globalLevel = lvl; }

LogLevel Log::getGlobalLevel() { return s_globalLevel; }

void Log::setTagLevel(const char *tag, LogLevel lvl)
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

LogLevel Log::getTagLevel(const char *tag)
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

void Log::setTimestampProvider(TimestampFn fn) { s_timestampProvider = fn; }

char Log::levelChar(LogLevel lvl)
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

bool Log::shouldEmit(LogLevel level, const char *tag)
{
	LogLevel tagLevel = getTagLevel(tag);
	LogLevel gate = (tagLevel != LogLevel::None) ? tagLevel : s_globalLevel;
	return static_cast<int>(level) <= static_cast<int>(gate);
}

void Log::lock() { s_logMutex.lock(); }

void Log::unlock() { s_logMutex.unlock(); }

void Log::vlog(LogLevel level, const char *tag, const char *fmt, va_list ap)
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

void Log::log(LogLevel level, const char *tag, const char *fmt, ...)
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

void *psram_malloc(std::size_t size)
{
	void *ptr = std::malloc(size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *psram_calloc(std::size_t num, std::size_t size)
{
	void *ptr = std::calloc(1, num * size + sizeof(std::size_t));
	if (!ptr)
		return nullptr;
	*static_cast<std::size_t *>(ptr) = num * size;
	return static_cast<char *>(ptr) + sizeof(std::size_t);
}

void *psram_realloc(void *ptr, std::size_t size)
{
	if (!ptr)
		return psram_malloc(size);
	void *original = static_cast<char *>(ptr) - sizeof(std::size_t);
	void *resized = std::realloc(original, size + sizeof(std::size_t));
	if (!resized)
		return nullptr;
	*static_cast<std::size_t *>(resized) = size;
	return static_cast<char *>(resized) + sizeof(std::size_t);
}

void psram_free(void *ptr)
{
	if (!ptr)
		return;
	std::free(static_cast<char *>(ptr) - sizeof(std::size_t));
}

std::size_t psram_allocated_size(void *ptr)
{
	if (!ptr)
		return 0;
	return *reinterpret_cast<std::size_t *>(static_cast<char *>(ptr) - sizeof(std::size_t));
}

}

namespace rtos {

// Host builds have no firmware image to describe; report compile-time
// fallbacks so code using AppInfo still runs in tests.
const AppInfo::Description &AppInfo::description()
{
	static const Description desc = [] {
		Description d{};
		std::snprintf(d.projectName, sizeof(d.projectName), "host");
		std::snprintf(d.buildDate, sizeof(d.buildDate), "%s", __DATE__);
		std::snprintf(d.buildTime, sizeof(d.buildTime), "%s", __TIME__);
		std::snprintf(d.sdkVersion, sizeof(d.sdkVersion), "linux");
		return d;
	}();
	return desc;
}

const AppInfo::Chip &AppInfo::chip()
{
	static const Chip chip = [] {
		Chip c{};
		std::snprintf(c.model, sizeof(c.model), "linux-host");
		c.cores = static_cast<uint8_t>(std::thread::hardware_concurrency());
		return c;
	}();
	return chip;
}

bool AppInfo::macAddress(uint8_t (&mac)[MacSize])
{
	std::memset(mac, 0, MacSize);
	return false;
}

}
// Platform bring-up: a Linux process needs no system services installed -
// signals/threads are ready at process start. Present so portable app code
// can unconditionally call rtos::backend::init().
bool rtos::backend::init() noexcept { return true; }
