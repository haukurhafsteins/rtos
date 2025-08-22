// RtosTask.hpp
#pragma once

constexpr uint32_t RTOS_TASK_WAIT_FOREVER = 0xffffffffUL;

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosTaskImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosTaskImpl.hpp"
#else
#error "Unsupported RTOS"
#endif

// Alias the platform-specific implementation
using RtosTask = RtosTaskImpl;
