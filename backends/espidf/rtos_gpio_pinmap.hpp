#pragma once

// Backend-private: how the ESP-IDF backend turns a Pin::make() pin id into a GPIO.
namespace rtos::gpio::espidf
{
    /// On ESP-IDF a pin id IS the native GPIO number (GPIO_NUM_x). Returns it
    /// unchanged when the chip has that GPIO (GPIO_IS_VALID_GPIO), else -1.
    /// There is no board table in between (HMO-74): the old one skipped the
    /// ESP32-S3's missing GPIO 22-25 and silently shifted every id above 21.
    int native_gpio(int pin_id);
}
