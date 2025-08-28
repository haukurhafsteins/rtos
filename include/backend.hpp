#pragma once
#include <cstdint>

using TaskFunction = void (*)(void*);

namespace rtos::backend {

// Opaque native handle for portability.
using TaskHandle = void*;

// Create a task. stack_size_bytes is in BYTES (the backend converts as needed).
bool task_create(TaskHandle& out_handle,
                 const char* name,
                 uint32_t stack_size_bytes,
                 uint32_t priority,
                 TaskFunction func,
                 void* arg) noexcept;

// Delete a task. (No-op on nullptr.)
void task_delete(TaskHandle handle) noexcept;

// Common utilities
void delay_ms(uint32_t ms) noexcept;
void yield() noexcept;

// Current task handle (opaque)
TaskHandle current_task() noexcept;

} // namespace rtos::backend
