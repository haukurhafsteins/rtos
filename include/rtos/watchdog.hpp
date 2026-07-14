#pragma once

#include <cstdint>
#include "rtos/time.hpp"

// ---------------------------------------------------------------------------
// Portable task watchdog abstraction.
//
// Model: one system-wide task watchdog with a single timeout configured at
// init(). Each task that wants to be supervised subscribes itself with add(),
// then must call feed() at least once per timeout period. A subscribed task
// that stops feeding triggers the timeout action (system reset/panic, or a
// diagnostic log — see Config::panic_on_timeout). Tasks unsubscribe with
// remove() before exiting.
//
// This is the intersection of the two native APIs:
//   - espidf: the Task Watchdog Timer (esp_task_wdt). add/remove/feed map to
//     esp_task_wdt_add(NULL) / esp_task_wdt_delete(NULL) /
//     esp_task_wdt_reset(). init() reconfigures the TWDT when it was already
//     started via CONFIG_ESP_TASK_WDT_INIT, and starts it otherwise.
//   - zephyr: the task_wdt subsystem (CONFIG_TASK_WDT) layered on the
//     hardware watchdog (devicetree alias "watchdog0", optional). Each add()
//     allocates a task_wdt channel with Config::timeout as its reload period;
//     the backend keeps a thread -> channel table so feed()/remove() work on
//     the calling thread like on espidf.
//   - linux: functional no-op for host builds and tests; every operation
//     succeeds.
//
// Each backend implements these functions in
// backends/<backend>/rtos_watchdog.cpp; the build system selects which
// backend is compiled.
// ---------------------------------------------------------------------------

namespace rtos::watchdog
{

struct Config
{
    /// Time a subscribed task may go without feed() before the watchdog
    /// fires.
    Millis timeout = Millis(5000);

    /// true:  a missed deadline resets the system (espidf: trigger_panic;
    ///        zephyr: task_wdt default action, reboot).
    /// false: a missed deadline is only logged.
    bool panic_on_timeout = true;

    /// espidf only: also watch the idle task of each core whose bit is set
    /// (bit N = core N). Ignored on other backends.
    uint32_t idle_core_mask = 0;
};

/// Start (or reconfigure, when the platform already started it) the task
/// watchdog. Returns true when the watchdog is running with `cfg` applied.
bool init(const Config& cfg);

/// Stop the task watchdog. Fails on espidf while tasks are still subscribed.
bool deinit();

/// Subscribe the calling task. It must feed() at least once per
/// Config::timeout from now on.
bool add();

/// Unsubscribe the calling task. Call before the task exits.
bool remove();

/// Reset the calling task's timeout. Only valid after add().
bool feed();

} // namespace rtos::watchdog
