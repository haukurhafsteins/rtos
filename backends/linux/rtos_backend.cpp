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
std::mutex s_logMutex;
}

namespace rtos {

void Log::lock() { s_logMutex.lock(); }

void Log::unlock() { s_logMutex.unlock(); }

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
