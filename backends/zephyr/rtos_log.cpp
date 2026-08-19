#include "rtos/Log.hpp"
#include "rtos/LogSinks.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace
{
K_MUTEX_DEFINE(s_logMutex);
}

namespace rtos
{
void Log::lock()
{
    k_mutex_lock(&s_logMutex, K_FOREVER);
}

void Log::unlock()
{
    k_mutex_unlock(&s_logMutex);
}

void ZephyrPrintkSink::write(LogLevel, const char *, const char *line, std::size_t length)
{
    printk("%.*s\n", static_cast<int>(length), line);
}
}
