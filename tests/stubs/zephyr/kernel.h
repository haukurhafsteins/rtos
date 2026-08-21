#pragma once

struct k_mutex
{
};

#define K_FOREVER 0
#define K_MUTEX_DEFINE(name) k_mutex name

inline int k_mutex_lock(k_mutex*, int)
{
    return 0;
}

inline int k_mutex_unlock(k_mutex*)
{
    return 0;
}
