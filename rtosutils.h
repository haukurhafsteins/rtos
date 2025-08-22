#pragma once

#include <stdio.h>
#include <esp_timer.h>

#define PRINT_FLOAT(name, value, period) \
    {                                    \
        static float last = 0;           \
        float now = esp_timer_get_time() * 1e-6; \
        if (now - last > period)          \
        {                                \
            last = now;                  \
            printf(name, value);          \
        }                                \
    }

#define get_time_in_seconds() (esp_timer_get_time() * 1e-6)

#define RUN_CODE_EVERY_START(period) \
    {                                 \
        static float last = 0;            \
        float now = get_time_in_seconds(); \
        if (now - last > period)          \
        {                                 \
            last = now;

#define RUN_CODE_EVERY_END() }}