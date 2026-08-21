#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

#include "rtos/backend.hpp"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "zephyr_gpio_test.hpp"

namespace
{
std::array<device, 2> devices{};
gpio_dt_spec configured{};
std::array<std::array<bool, 32>, 2> levels{};
const device* callbackPort = nullptr;
gpio_callback* installedCallback = nullptr;
bool queueSendWasIsr = false;

struct QueueState
{
    std::size_t capacity = 0;
    std::size_t itemSize = 0;
    std::deque<std::vector<std::byte>> items;
};
}

const device* zephyr_test_device(int id)
{
    auto& value = devices.at(static_cast<std::size_t>(id));
    value.id = id;
    value.ready = true;
    return &value;
}

gpio_dt_spec zephyr_test_gpio_spec(int node, int property)
{
    if (node == DT_ALIAS_sw0)
        return {zephyr_test_device(0), 7, GPIO_ACTIVE_HIGH};
    if (node == DT_ALIAS_sw1)
        return {zephyr_test_device(0), 4, GPIO_ACTIVE_HIGH};
    if (node == DT_ALIAS_led0)
        return {zephyr_test_device(0), 25, GPIO_ACTIVE_HIGH};

    switch (property)
    {
    case ZEPHYR_GPIO_PROP_imu_int1_gpios:
        return {zephyr_test_device(0), 26, GPIO_ACTIVE_HIGH};
    case ZEPHYR_GPIO_PROP_imu_int2_gpios:
        return {zephyr_test_device(0), 27, GPIO_ACTIVE_HIGH};
    case ZEPHYR_GPIO_PROP_pmic_int_gpios:
        return {zephyr_test_device(0), 28, GPIO_ACTIVE_HIGH};
    case ZEPHYR_GPIO_PROP_bat_alrt_gpios:
        return {zephyr_test_device(0), 24, GPIO_ACTIVE_HIGH};
    case ZEPHYR_GPIO_PROP_haptic_en_gpios:
        return {zephyr_test_device(1), 15, GPIO_ACTIVE_HIGH};
    default:
        return {};
    }
}

void zephyr_gpio_test::reset()
{
    configured = {};
    levels = {};
    callbackPort = nullptr;
    installedCallback = nullptr;
    queueSendWasIsr = false;
}

const gpio_dt_spec& zephyr_gpio_test::lastConfigured()
{
    return configured;
}

void zephyr_gpio_test::fire(int port, uint32_t pin, bool level)
{
    levels.at(static_cast<std::size_t>(port)).at(pin) = level;
    if (installedCallback != nullptr && callbackPort->id == port)
        installedCallback->handler(callbackPort, installedCallback, 1U << pin);
}

bool zephyr_gpio_test::lastQueueSendWasIsr()
{
    return queueSendWasIsr;
}

int gpio_pin_configure_dt(const gpio_dt_spec* spec, gpio_flags_t)
{
    configured = *spec;
    return 0;
}

int gpio_pin_get_dt(const gpio_dt_spec* spec)
{
    return levels.at(static_cast<std::size_t>(spec->port->id)).at(spec->pin);
}

int gpio_pin_get_dt(const gpio_dt_spec* spec, int* value)
{
    *value = gpio_pin_get_dt(spec);
    return 0;
}

int gpio_pin_set_dt(const gpio_dt_spec*, int)
{
    return 0;
}

int gpio_pin_toggle_dt(const gpio_dt_spec*)
{
    return 0;
}

int gpio_pin_interrupt_configure_dt(const gpio_dt_spec*, gpio_flags_t)
{
    return 0;
}

void gpio_init_callback(
    gpio_callback* callback,
    gpio_callback_handler_t handler,
    gpio_port_pins_t pinMask)
{
    callback->handler = handler;
    callback->pin_mask = pinMask;
}

int gpio_add_callback(const device* port, gpio_callback* callback)
{
    callbackPort = port;
    installedCallback = callback;
    return 0;
}

int gpio_remove_callback(const device*, gpio_callback* callback)
{
    if (installedCallback == callback)
    {
        callbackPort = nullptr;
        installedCallback = nullptr;
    }
    return 0;
}

uint64_t k_uptime_ticks()
{
    return 0;
}

namespace rtos::backend
{
bool queue_create(
    QueueHandle& outHandle, std::size_t length, std::size_t itemSize) noexcept
{
    outHandle = new QueueState{length, itemSize, {}};
    return true;
}

void queue_delete(QueueHandle handle) noexcept
{
    delete static_cast<QueueState*>(handle);
}

bool queue_send_isr(
    QueueHandle handle, const void* item, bool* higherPriorityTaskWoken) noexcept
{
    if (higherPriorityTaskWoken != nullptr)
        *higherPriorityTaskWoken = false;
    auto* queue = static_cast<QueueState*>(handle);
    if (queue == nullptr || item == nullptr || queue->items.size() == queue->capacity)
        return false;
    std::vector<std::byte> bytes(queue->itemSize);
    std::memcpy(bytes.data(), item, queue->itemSize);
    queue->items.push_back(std::move(bytes));
    queueSendWasIsr = true;
    return true;
}

bool queue_receive(
    QueueHandle handle, void* item, uint32_t) noexcept
{
    auto* queue = static_cast<QueueState*>(handle);
    if (queue == nullptr || item == nullptr || queue->items.empty())
        return false;
    std::memcpy(item, queue->items.front().data(), queue->itemSize);
    queue->items.pop_front();
    return true;
}
}
