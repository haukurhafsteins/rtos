// RtosQueue.hpp
#pragma once

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosQueueImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosQueueImpl.hpp"
#else
// #error "Unsupported RTOS"
#endif

// Alias the platform-specific implementation

template<typename T>
using RtosQueue = RtosQueueImpl<T>;
