#pragma once

// Host stand-in for ESP-IDF's driver/gpio.h: only the validity macros the
// pin-id resolver uses, with the ESP32-S3 values from ESP-IDF v5.5
// components/soc/esp32s3/include/soc/soc_caps.h (GPIO 0..48 exist except 22..25).
#include <cstdint>

#define SOC_GPIO_PIN_COUNT 49
#define SOC_GPIO_VALID_GPIO_MASK \
    (0x1FFFFFFFFFFFFULL & ~((1ULL << 22) | (1ULL << 23) | (1ULL << 24) | (1ULL << 25)))

#define GPIO_IS_VALID_GPIO(gpio_num) \
    ((gpio_num >= 0) && (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
