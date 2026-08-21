#pragma once

#include <cstddef>
#include <cstdint>

struct k_work
{
    void (*handler)(k_work*) = nullptr;
};

inline void k_work_init(k_work* work, void (*handler)(k_work*))
{
    work->handler = handler;
}

inline int k_work_submit(k_work* work)
{
    work->handler(work);
    return 1;
}

uint64_t k_uptime_ticks();

inline uint64_t k_ticks_to_us_near64(uint64_t ticks)
{
    return ticks;
}
