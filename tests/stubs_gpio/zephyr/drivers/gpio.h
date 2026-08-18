#pragma once

#include <cstdint>

#include <zephyr/device.h>

using gpio_flags_t = uint32_t;
using gpio_port_pins_t = uint32_t;

constexpr gpio_flags_t GPIO_ACTIVE_HIGH = 0;
constexpr gpio_flags_t GPIO_ACTIVE_LOW = 1U << 0;
constexpr gpio_flags_t GPIO_INPUT = 1U << 1;
constexpr gpio_flags_t GPIO_OUTPUT = 1U << 2;
constexpr gpio_flags_t GPIO_OPEN_DRAIN = 1U << 3;
constexpr gpio_flags_t GPIO_PULL_UP = 1U << 4;
constexpr gpio_flags_t GPIO_PULL_DOWN = 1U << 5;
constexpr gpio_flags_t GPIO_INT_DISABLE = 0;
constexpr gpio_flags_t GPIO_INT_EDGE_RISING = 1U << 6;
constexpr gpio_flags_t GPIO_INT_EDGE_FALLING = 1U << 7;
constexpr gpio_flags_t GPIO_INT_EDGE_BOTH =
    GPIO_INT_EDGE_RISING | GPIO_INT_EDGE_FALLING;
constexpr gpio_flags_t GPIO_INT_LEVEL_HIGH = 1U << 8;
constexpr gpio_flags_t GPIO_INT_LEVEL_LOW = 1U << 9;

struct gpio_dt_spec
{
    const device* port = nullptr;
    uint32_t pin = 0;
    gpio_flags_t dt_flags = 0;
};

struct gpio_callback;
using gpio_callback_handler_t = void (*)(
    const device*, gpio_callback*, gpio_port_pins_t);

struct gpio_callback
{
    gpio_callback_handler_t handler = nullptr;
    gpio_port_pins_t pin_mask = 0;
};

inline bool gpio_is_ready_dt(const gpio_dt_spec* spec)
{
    return spec != nullptr && device_is_ready(spec->port);
}

gpio_dt_spec zephyr_test_gpio_spec(int node, int property);

#define ZEPHYR_GPIO_PROP_gpios 0
#define ZEPHYR_GPIO_PROP_imu_int1_gpios 1
#define ZEPHYR_GPIO_PROP_imu_int2_gpios 2
#define ZEPHYR_GPIO_PROP_pmic_int_gpios 3
#define ZEPHYR_GPIO_PROP_bat_alrt_gpios 4
#define ZEPHYR_GPIO_PROP_haptic_en_gpios 5
#define ZEPHYR_GPIO_PROP(property) ZEPHYR_GPIO_PROP_##property
#define GPIO_DT_SPEC_GET(node, property) \
    zephyr_test_gpio_spec(node, ZEPHYR_GPIO_PROP(property))

int gpio_pin_configure_dt(const gpio_dt_spec* spec, gpio_flags_t flags);
int gpio_pin_get_dt(const gpio_dt_spec* spec);
int gpio_pin_get_dt(const gpio_dt_spec* spec, int* value);
int gpio_pin_set_dt(const gpio_dt_spec* spec, int value);
int gpio_pin_toggle_dt(const gpio_dt_spec* spec);
int gpio_pin_interrupt_configure_dt(
    const gpio_dt_spec* spec, gpio_flags_t flags);
void gpio_init_callback(
    gpio_callback* callback,
    gpio_callback_handler_t handler,
    gpio_port_pins_t pinMask);
int gpio_add_callback(const device* port, gpio_callback* callback);
int gpio_remove_callback(const device* port, gpio_callback* callback);
