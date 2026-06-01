#pragma once

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosI2CImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosI2CImpl.hpp"
#elif defined(USE_LINUX)
#include "Linux/RtosI2CImpl.hpp"
#else
#error "Unsupported RTOS"
#endif

using RtosI2CBus    = RtosI2CBusImpl;
using RtosI2CDevice = RtosI2CDeviceImpl;