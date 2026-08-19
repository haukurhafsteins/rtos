#pragma once

#include <cstddef>
#include <cstdint>

struct k_timeout_t
{
    std::int64_t milliseconds;
};

inline constexpr k_timeout_t K_NO_WAIT{0};
inline constexpr k_timeout_t K_FOREVER{-1};
#define K_MSEC(value) k_timeout_t{value}

struct k_spinlock
{
    int unused;
};

using k_spinlock_key_t = unsigned int;

struct k_sem
{
    unsigned int count;
    unsigned int limit;
};

struct k_mutex
{
    int unused;
};

extern "C" {
k_spinlock_key_t k_spin_lock(k_spinlock *lock);
void k_spin_unlock(k_spinlock *lock, k_spinlock_key_t key);

int k_sem_init(k_sem *sem, unsigned int initial_count, unsigned int limit);
int k_sem_take(k_sem *sem, k_timeout_t timeout);
void k_sem_give(k_sem *sem);
void k_sem_reset(k_sem *sem);

int k_mutex_init(k_mutex *mutex);
int k_mutex_lock(k_mutex *mutex, k_timeout_t timeout);
int k_mutex_unlock(k_mutex *mutex);

void *k_malloc(std::size_t size);
void k_free(void *memory);
std::int64_t k_uptime_get();
}
