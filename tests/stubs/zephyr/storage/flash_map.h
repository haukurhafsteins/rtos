#pragma once

#include <zephyr/device.h>

extern const device rtos_test_flash_device;

#define FIXED_PARTITION_DEVICE(partition) (&rtos_test_flash_device)
#define FIXED_PARTITION_OFFSET(partition) 0
#define FIXED_PARTITION_SIZE(partition) (64 * 1024)
