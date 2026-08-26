#include "zephyr_watchdog_test.hpp"

#include <cstdarg>

#include <zephyr/kernel.h>
#include <zephyr/task_wdt/task_wdt.h>

namespace
{
device hardwareWatchdog{17};
const device *initDevice = nullptr;
int currentThreadStorage = 0;
}

const device *zephyr_watchdog_test_device()
{
    return &hardwareWatchdog;
}

const device *zephyr_watchdog_init_device()
{
    return initDevice;
}

void zephyr_watchdog_reset_test_state()
{
    initDevice = nullptr;
}

int task_wdt_init(const device *hardware_watchdog)
{
    initDevice = hardware_watchdog;
    return 0;
}

int task_wdt_add(uint32_t, task_wdt_callback_t, void *)
{
    return 0;
}

int task_wdt_delete(int)
{
    return 0;
}

int task_wdt_feed(int)
{
    return 0;
}

k_tid_t k_current_get()
{
    return &currentThreadStorage;
}

const char *k_thread_name_get(k_tid_t)
{
    return "test";
}

extern "C" int printk(const char *, ...)
{
    return 0;
}
