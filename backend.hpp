#pragma once
#include <cstdint>

namespace rtos::backend {

struct ThreadHandle;
struct QueueHandle;
struct SemaphoreHandle;
// opaque forward declarations

struct ThreadSpec { const char* name; size_t stack; int prio; void* arg; void (*entry)(void*); };

ThreadHandle* thread_create(const ThreadSpec& spec);  // starts running
void          thread_start_gate_wait();               // optional: entry gate wait (TLS/arg)
void          thread_start_gate_open(ThreadHandle*);  // open gate from start()

// …plus queue/msgbuf primitives you need, ISR variants if applicable

} // namespace rtos::backend