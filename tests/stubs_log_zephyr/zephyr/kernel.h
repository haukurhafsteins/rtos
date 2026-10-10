#pragma once
#include <mutex>
struct k_mutex { std::mutex value; };
#define K_MUTEX_DEFINE(name) k_mutex name
#define K_FOREVER -1
inline int k_mutex_lock(k_mutex *mutex, int) { mutex->value.lock(); return 0; }
inline int k_mutex_unlock(k_mutex *mutex) { mutex->value.unlock(); return 0; }
inline thread_local bool testInIsr = false;
inline bool k_is_in_isr() { return testInIsr; }
