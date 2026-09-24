#include "rtos_gpio_pinmap.hpp"

#include <driver/gpio.h>

int rtos::gpio::espidf::native_gpio(int pin_id)
{
    // GPIO_IS_VALID_GPIO shifts 1ULL by pin_id, so keep the shift in range first.
    if (pin_id < 0 || pin_id >= SOC_GPIO_PIN_COUNT)
        return -1;
    return GPIO_IS_VALID_GPIO(pin_id) ? pin_id : -1;
}
