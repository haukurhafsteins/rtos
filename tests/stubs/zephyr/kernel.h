#pragma once

#include <cstdint>

struct k_timeout_t
{
    std::int64_t milliseconds;
};

struct k_mutex
{
    int unused;
};

#define K_FOREVER k_timeout_t{-1}
#define K_MSEC(value) k_timeout_t{value}
#define K_MUTEX_DEFINE(name) struct k_mutex name{}

extern "C" {
std::int64_t k_uptime_ticks();
std::uint64_t k_ticks_to_us_floor64(std::int64_t ticks);
int k_sleep(k_timeout_t timeout);
int k_usleep(std::int32_t microseconds);
int k_mutex_lock(k_mutex *mutex, k_timeout_t timeout);
int k_mutex_unlock(k_mutex *mutex);
[[noreturn]] void k_panic();
}
