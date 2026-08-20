#pragma once

#include <cstddef>
#include <cstdint>

#define CONFIG_NUM_PREEMPT_PRIORITIES 25

struct k_timeout_t
{
    std::int64_t milliseconds;
};

inline constexpr k_timeout_t K_NO_WAIT{0};
inline constexpr k_timeout_t K_FOREVER{-1};
#define K_MSEC(value) k_timeout_t{value}

struct k_thread
{
    int identifier;
};

using k_tid_t = k_thread *;
using k_thread_entry_t = void (*)(void *, void *, void *);
using k_thread_stack_t = unsigned char;

#define K_THREAD_STACK_ARRAY_DEFINE(symbol, count, size) \
    alignas(16) k_thread_stack_t symbol[count][size]
#define K_THREAD_STACK_SIZEOF(symbol) sizeof(symbol)

struct k_spinlock
{
    int unused;
};

using k_spinlock_key_t = unsigned int;

struct k_msgq
{
    void *state;
};

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
k_tid_t k_thread_create(
    k_thread *thread,
    k_thread_stack_t *stack,
    std::size_t stack_size,
    k_thread_entry_t entry,
    void *p1,
    void *p2,
    void *p3,
    int priority,
    std::uint32_t options,
    k_timeout_t delay);
int k_thread_name_set(k_tid_t thread, const char *name);
int k_thread_join(k_tid_t thread, k_timeout_t timeout);
void k_thread_abort(k_tid_t thread);
k_tid_t k_current_get();
int k_sleep(k_timeout_t timeout);
void k_yield();

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
int k_msgq_alloc_init(k_msgq *queue, std::size_t item_size, std::uint32_t length);
int k_msgq_cleanup(k_msgq *queue);
int k_msgq_put(k_msgq *queue, const void *item, k_timeout_t timeout);
int k_msgq_get(k_msgq *queue, void *item, k_timeout_t timeout);
void k_msgq_purge(k_msgq *queue);
std::uint32_t k_msgq_num_free_get(k_msgq *queue);
std::uint32_t k_msgq_num_used_get(k_msgq *queue);
std::int64_t k_uptime_get();
}

#define K_PRIO_PREEMPT(priority) (priority)
