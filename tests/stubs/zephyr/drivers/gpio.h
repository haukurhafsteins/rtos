#pragma once

#include <cstdint>

#include <zephyr/device.h>

constexpr uint32_t GPIO_ACTIVE_LOW = 1;

struct gpio_dt_spec
{
    const device* port = nullptr;
    uint32_t pin = 0;
    uint32_t dt_flags = 0;
};

#define GPIO_DT_SPEC_GET_BY_IDX(node, prop, index) \
    {zephyr_test_device(11), 7, GPIO_ACTIVE_LOW}

inline bool gpio_is_ready_dt(const gpio_dt_spec* value)
{
    return value != nullptr && device_is_ready(value->port);
}
