#pragma once

#include <zephyr/device.h>

#define DT_ALIAS(name) 1
#define DEVICE_DT_GET_OR_NULL(node) zephyr_watchdog_test_device()
