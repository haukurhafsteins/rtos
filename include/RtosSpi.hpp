#pragma once

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosSpiImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosSpiImpl.hpp"
#elif defined(USE_LINUX)
#include "Linux/RtosSpiImpl.hpp"
#else
#error "Unsupported RTOS"
#endif

using RtosSpiBus    = RtosSpiBusImpl;
using RtosSpiDevice = RtosSpiDeviceImpl;
