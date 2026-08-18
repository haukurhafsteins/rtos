#pragma once

#include <zephyr/device.h>

#define DT_NODELABEL(label) DT_NODELABEL_##label
#define DT_NODELABEL_spi3 3
#define DT_NODELABEL_i2c1 1
#define DT_PINCTRL_BY_NAME(node, name, index) node
#define DT_CHILD(node, child) node
#define DT_PROP_BY_IDX(node, prop, index) \
    ((node) == DT_NODELABEL_spi3 \
            ? ((index) == 0 ? 36 : ((index) == 1 ? 37 : 38)) \
            : ((index) == 0 ? 35 : 34))
#define DT_PROP(node, prop) DT_PROP_##prop(node)
#define DT_PROP_bias_pull_up(node) 0
#define DT_PROP_BY_PHANDLE_IDX(node, prop, index, cell) 1
#define DT_GPIO_PIN_BY_IDX(node, prop, index) 7
#define DEVICE_DT_GET(node) zephyr_test_device(node)
