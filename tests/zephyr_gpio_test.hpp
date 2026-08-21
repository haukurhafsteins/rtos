#pragma once

#include <cstdint>

#include <zephyr/drivers/gpio.h>

namespace zephyr_gpio_test
{
void reset();
const gpio_dt_spec& lastConfigured();
void fire(int port, uint32_t pin, bool level);
bool lastQueueSendWasIsr();
}
