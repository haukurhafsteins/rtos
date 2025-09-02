// RtosTask.hpp
#pragma once

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosI2CImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosI2CImpl.hpp"
#else
#error "Unsupported RTOS"
#endif

// Alias the platform-specific implementation
using RtosI2C = RtosI2CImpl;