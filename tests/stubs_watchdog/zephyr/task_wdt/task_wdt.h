#pragma once

#include <cstdint>

#include <zephyr/device.h>

using task_wdt_callback_t = void (*)(int channel_id, void *user_data);

int task_wdt_init(const device *hardware_watchdog);
int task_wdt_add(uint32_t reload_period, task_wdt_callback_t callback,
                 void *user_data);
int task_wdt_delete(int channel_id);
int task_wdt_feed(int channel_id);
