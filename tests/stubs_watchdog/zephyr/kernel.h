#pragma once

using k_tid_t = void *;
using k_spinlock_key_t = unsigned int;

struct k_spinlock
{
};

inline k_spinlock_key_t k_spin_lock(k_spinlock *)
{
    return 0;
}

inline void k_spin_unlock(k_spinlock *, k_spinlock_key_t)
{
}

k_tid_t k_current_get();
const char *k_thread_name_get(k_tid_t thread);
