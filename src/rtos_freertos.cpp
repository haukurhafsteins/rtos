#include "backend.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

namespace {

// FreeRTOS expects stack depth in words (StackType_t), not bytes.
constexpr uint32_t bytes_to_stack_depth(uint32_t bytes) {
    return (bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
}

} // namespace

namespace rtos::backend {

bool task_create(TaskHandle& out_handle,
                 const char* name,
                 uint32_t stack_size_bytes,
                 uint32_t priority,
                 TaskFunction func,
                 void* arg) noexcept
{
    TaskHandle_t native = nullptr;
    const uint32_t depth = bytes_to_stack_depth(stack_size_bytes);
    BaseType_t res = xTaskCreate(func, name, depth, arg, priority, &native);
    if (res != pdPASS) {
        out_handle = nullptr;
        return false;
    }
    out_handle = static_cast<TaskHandle>(native);
    return true;
}

void task_delete(TaskHandle handle) noexcept {
    if (handle) {
        vTaskDelete(static_cast<TaskHandle_t>(handle));
    }
}

void delay_ms(uint32_t ms) noexcept {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void yield() noexcept {
    taskYIELD();
}

TaskHandle current_task() noexcept {
    return static_cast<TaskHandle>(xTaskGetCurrentTaskHandle());
}

} // namespace rtos::backend
