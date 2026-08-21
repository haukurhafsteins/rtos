#pragma once

#include <zephyr/device.h>

#define DT_ALIAS(alias) DT_ALIAS_##alias
#define DT_ALIAS_sw0 100
#define DT_ALIAS_sw1 101
#define DT_ALIAS_led0 102

#define DT_PATH(path) DT_PATH_##path
#define DT_PATH_zephyr_user 200

#define DT_NODELABEL(label) DT_NODELABEL_##label
#define DT_NODELABEL_gpio0 0
#define DT_NODELABEL_gpio1 1

#define DEVICE_DT_GET(node) zephyr_test_device(node)
