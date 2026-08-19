#pragma once

#include <cassert>

#if defined(__ZEPHYR__)

#include <zephyr/kernel.h>

namespace rtos
{

class Mutex
{
public:
    Mutex() noexcept
    {
        const int result = k_mutex_init(&_mutex);
        assert(result == 0);
        (void)result;
    }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(Mutex&&) = delete;

    void lock() noexcept
    {
        const int result = k_mutex_lock(&_mutex, K_FOREVER);
        assert(result == 0);
        (void)result;
    }

    void unlock() noexcept
    {
        const int result = k_mutex_unlock(&_mutex);
        assert(result == 0);
        (void)result;
    }

private:
    k_mutex _mutex{};
};

class LockGuard
{
public:
    explicit LockGuard(Mutex& mutex) noexcept : _mutex(mutex)
    {
        _mutex.lock();
    }

    ~LockGuard()
    {
        _mutex.unlock();
    }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    Mutex& _mutex;
};

} // namespace rtos

#else

#include <mutex>

namespace rtos
{

using Mutex = std::mutex;
using LockGuard = std::lock_guard<Mutex>;

} // namespace rtos

#endif
