#pragma once

#include <stdio.h>
#include <esp_timer.h>
#include "time.hpp"

#define PRINT_FLOAT_EVERY_MS(_dbg_name, _dbg_value, _dbg_period) \
    {                                    \
        static Millis _dbg_last = Millis(0);           \
        Millis _dbg_now = rtos::time::now_ms(); \
        if (_dbg_now - _dbg_last > Millis(static_cast<long long>(_dbg_period))) \
        {                                \
            _dbg_last = _dbg_now;                  \
            printf(_dbg_name, _dbg_value);          \
        }                                \
    }

#define RUN_CODE_EVERY_MS(_dbg_period) \
    {                                 \
        static Millis _dbg_last = Millis(0);  \
        Millis _dbg_now = rtos::time::now_ms(); \
        if (_dbg_now - _dbg_last > Millis(static_cast<long long>(_dbg_period)))  \
        {                                 \
            _dbg_last = _dbg_now;

#define RUN_CODE_EVERY_END() }}