#pragma once

#include <stdio.h>
#include <esp_timer.h>
#include "time.hpp"

#define PRINT_FLOAT(name, value, period) \
    {                                    \
        static float last = 0;           \
        float now = (float) (1e-6 * rtos::time::now_ms().count()); \
        if (now - last > period)          \
        {                                \
            last = now;                  \
            printf(name, value);          \
        }                                \
    }

#define RUN_CODE_EVERY_START(period) \
    {                                 \
        static float last = 0;            \
        float now = (float) (1e-6 * rtos::time::now_ms().count()); \
        if (now - last > period)          \
        {                                 \
            last = now;

#define RUN_CODE_EVERY_END() }}