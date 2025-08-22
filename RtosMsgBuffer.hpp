// RtosMsgBuffer.hpp
#pragma once

#if defined(USE_FREERTOS)
#include "FreeRTOS/RtosMessageBufferImpl.hpp"
#elif defined(USE_ZEPHYR)
#include "Zephyr/RtosMessageBufferImpl.hpp"
#else
#error "Unsupported RTOS"
#endif

using RtosMsgBuffer = RtosMessageBufferImpl;
