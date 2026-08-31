#pragma once

#include <zephyr/device.h>

const device *zephyr_watchdog_init_device();
void zephyr_watchdog_reset_test_state();
