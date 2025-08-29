#pragma once
#include <cstdint>
#include <cstddef>
#include "time.hpp"

using TaskFunction = void (*)(void*);
using Millis = rtos::time::Millis;
namespace rtos::backend {

// ========= Common =========
inline constexpr Millis RTOS_WAIT_FOREVER = Millis::max();

// ===== Task backend (already present) =====
using TaskHandle = void*;
bool task_create(TaskHandle& out_handle, const char* name,
                 uint32_t stack_size_bytes, uint32_t priority,
                 TaskFunction func, void* arg) noexcept;
void task_delete(TaskHandle handle) noexcept;
void delay_ms(uint32_t ms) noexcept;
void yield() noexcept;
TaskHandle current_task() noexcept;

// ===== Queue backend (already present) =====
using QueueHandle = void*;
bool queue_create(QueueHandle& out_handle, std::size_t length, std::size_t item_size) noexcept;
void queue_delete(QueueHandle handle) noexcept;
bool queue_send(QueueHandle handle, const void* item, uint32_t timeout_ms) noexcept;
bool queue_receive(QueueHandle handle, void* out_item, uint32_t timeout_ms) noexcept;
bool queue_send_isr(QueueHandle handle, const void* item, bool* hp_task_woken) noexcept;
bool queue_receive_isr(QueueHandle handle, void* out_item, bool* hp_task_woken) noexcept;
std::size_t queue_count(QueueHandle handle) noexcept;
std::size_t queue_spaces(QueueHandle handle) noexcept;
bool queue_reset(QueueHandle handle) noexcept;

// ===== Message buffer backend (new) =====
using MsgBufferHandle = void*;

bool   msgbuf_create(MsgBufferHandle& out_handle, std::size_t capacity_bytes) noexcept;
void   msgbuf_delete(MsgBufferHandle handle) noexcept;

std::size_t msgbuf_send(MsgBufferHandle handle,
                        const void* data, std::size_t bytes,
                        Millis timeout_ms) noexcept;

std::size_t msgbuf_receive(MsgBufferHandle handle,
                           void* out, std::size_t max_bytes,
                           Millis timeout_ms) noexcept;

std::size_t msgbuf_send_isr(MsgBufferHandle handle,
                            const void* data, std::size_t bytes,
                            bool* hp_task_woken) noexcept;

std::size_t msgbuf_receive_isr(MsgBufferHandle handle,
                               void* out, std::size_t max_bytes,
                               bool* hp_task_woken) noexcept;

std::size_t msgbuf_next_len(MsgBufferHandle handle) noexcept;      // size of next pending message
std::size_t msgbuf_space_available(MsgBufferHandle handle) noexcept;
bool        msgbuf_reset(MsgBufferHandle handle) noexcept;

} // namespace rtos::backend
