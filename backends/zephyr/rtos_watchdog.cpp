// Zephyr backend for rtos::watchdog (task_wdt subsystem, CONFIG_TASK_WDT).
//
// task_wdt is channel-based: add() allocates a channel with the configured
// timeout as its reload period, and a thread -> channel table lets feed()
// and remove() operate on the calling thread, matching the espidf model.
// The hardware watchdog behind task_wdt is taken from the devicetree alias
// "watchdog0" when present; task_wdt falls back to a pure software timer
// otherwise.

#include "rtos/watchdog.hpp"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/sys/printk.h>

namespace
{

struct Entry
{
    k_tid_t thread  = nullptr;
    int     channel = -1;
};

constexpr int MAX_TASKS = 16;
Entry table[MAX_TASKS];
struct k_spinlock table_lock;

uint32_t reload_ms        = 5000;
bool     panic_on_timeout = true;
bool     initialised      = false;

Entry* find_locked(k_tid_t thread)
{
    for (Entry& e : table)
        if (e.thread == thread)
            return &e;
    return nullptr;
}

// Installed as the channel callback when panic_on_timeout is false; without
// a callback task_wdt's default action is a cold reboot.
void log_only_expiry(int /*channel_id*/, void* user_data)
{
    auto thread = static_cast<k_tid_t>(user_data);
    const char* name = k_thread_name_get(thread);
    printk("rtos::watchdog: task %s missed its deadline\n", name ? name : "<unnamed>");
}

} // namespace

bool rtos::watchdog::init(const Config& cfg)
{
    const struct device* hw_wdt = DEVICE_DT_GET_OR_NULL(DT_ALIAS(watchdog0));

    reload_ms        = static_cast<uint32_t>(cfg.timeout.count());
    panic_on_timeout = cfg.panic_on_timeout;
    // cfg.idle_core_mask: espidf-only, ignored.

    if (task_wdt_init(hw_wdt) != 0)
        return false;

    initialised = true;
    return true;
}

bool rtos::watchdog::deinit()
{
    // task_wdt has no teardown; release the channels this wrapper allocated.
    k_spinlock_key_t key = k_spin_lock(&table_lock);
    for (Entry& e : table)
    {
        if (e.thread != nullptr)
        {
            task_wdt_delete(e.channel);
            e = Entry{};
        }
    }
    k_spin_unlock(&table_lock, key);

    initialised = false;
    return true;
}

bool rtos::watchdog::add()
{
    if (!initialised)
        return false;

    k_tid_t self = k_current_get();

    task_wdt_callback_t callback = panic_on_timeout ? nullptr : log_only_expiry;
    int channel = task_wdt_add(reload_ms, callback, self);
    if (channel < 0)
        return false;

    k_spinlock_key_t key = k_spin_lock(&table_lock);
    Entry* slot = find_locked(self);
    if (slot == nullptr)
        slot = find_locked(nullptr); // free entry
    if (slot != nullptr)
    {
        // Re-adding replaces the previous subscription.
        if (slot->thread == self)
            task_wdt_delete(slot->channel);
        slot->thread  = self;
        slot->channel = channel;
    }
    k_spin_unlock(&table_lock, key);

    if (slot == nullptr)
    {
        task_wdt_delete(channel);
        return false;
    }
    return true;
}

bool rtos::watchdog::remove()
{
    k_spinlock_key_t key = k_spin_lock(&table_lock);
    Entry* entry = find_locked(k_current_get());
    int channel = entry != nullptr ? entry->channel : -1;
    if (entry != nullptr)
        *entry = Entry{};
    k_spin_unlock(&table_lock, key);

    return channel >= 0 && task_wdt_delete(channel) == 0;
}

bool rtos::watchdog::feed()
{
    k_spinlock_key_t key = k_spin_lock(&table_lock);
    Entry* entry = find_locked(k_current_get());
    int channel = entry != nullptr ? entry->channel : -1;
    k_spin_unlock(&table_lock, key);

    return channel >= 0 && task_wdt_feed(channel) == 0;
}
