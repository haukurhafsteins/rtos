#pragma once

#include <stdio.h>
#include <esp_timer.h>
#include "time.hpp"

#define PRINT_FLOAT_EVERY_MS(name, value, period) \
    {                                    \
        static Millis last = Millis(0);           \
        Millis now = rtos::time::now_ms(); \
        if (now - last > Millis(period)) \
        {                                \
            last = now;                  \
            printf(name, value);          \
        }                                \
    }

#define RUN_CODE_EVERY_MS(period) \
    {                                 \
        static Millis last = Millis(0);  \
        Millis now = rtos::time::now_ms(); \
        if (now - last > Millis(period))  \
        {                                 \
            last = now;

#define RUN_CODE_EVERY_END() }}