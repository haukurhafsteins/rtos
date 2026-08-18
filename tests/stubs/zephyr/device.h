#pragma once

struct device
{
    int id = 0;
    bool ready = true;
};

const device* zephyr_test_device(int id);

inline bool device_is_ready(const device* value)
{
    return value != nullptr && value->ready;
}
