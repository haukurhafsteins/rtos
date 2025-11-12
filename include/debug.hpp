#pragma once

#include <type_traits>
#include <stdio.h>
#include <esp_timer.h>
#include "time.hpp"

#define PRINT_FLOAT_EVERY_Millis(_dbg_period, _dbg_name, _dbg_value) \
    {                                    \
        static Millis _dbg_last = Millis(0);           \
        Millis _dbg_now = rtos::time::now_ms(); \
        if (_dbg_now - _dbg_last > Millis(_dbg_period)) \
        {                                \
            _dbg_last = _dbg_now;                  \
            printf(_dbg_name, _dbg_value);          \
        }                                \
    }

#define RUN_CODE_EVERY_Millis(_dbg_period) \
    { \
        static_assert(std::is_same_v<decltype(_dbg_period), Millis>, "RUN_CODE_EVERY_Millis requires a Millis argument"); \
        static Millis _dbg_last = Millis(0);  \
        Millis _dbg_now = rtos::time::now_ms(); \
        if (_dbg_now - _dbg_last > Millis(_dbg_period))  \
        {                                 \
            _dbg_last = _dbg_now;

#define RUN_CODE_EVERY_END() }}