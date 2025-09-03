#include "backend.hpp"
#include "time.hpp"
#include "rtos_assert.hpp"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
}

using rtos::time::Millis;

namespace
{
    //-----------------------------------------------------------------------------
    // Tasks
    //-----------------------------------------------------------------------------
    // FreeRTOS expects stack depth in words (StackType_t), not bytes.
    constexpr uint32_t bytes_to_stack_depth(uint32_t bytes)
    {
        return (bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    }

} // namespace

namespace rtos::backend
{
    bool task_create(TaskHandle &out_handle,
                     const char *name,
                     uint32_t stack_size_bytes,
                     uint32_t priority,
                     TaskFunction func,
                     void *arg) noexcept
    {
        TaskHandle_t native = nullptr;
        const uint32_t depth = bytes_to_stack_depth(stack_size_bytes);
        BaseType_t res = xTaskCreate(func, name, depth, arg, priority, &native);
        if (res != pdPASS)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<TaskHandle>(native);
        return true;
    }

    void task_delete(TaskHandle handle) noexcept
    {
        if (handle)
        {
            vTaskDelete(static_cast<TaskHandle_t>(handle));
        }
    }

    void delay_ms(uint32_t ms) noexcept { vTaskDelay(pdMS_TO_TICKS(ms)); }
    void yield() noexcept { taskYIELD(); }

    TaskHandle current_task() noexcept
    {
        return static_cast<TaskHandle>(xTaskGetCurrentTaskHandle());
    }

    //-----------------------------------------------------------------------------
    // Time
    //-----------------------------------------------------------------------------
    inline TickType_t to_ticks(Millis timeout_ms)
    {
        if (timeout_ms == Millis::max())
        {
            return portMAX_DELAY;
        }
        return pdMS_TO_TICKS(timeout_ms.count());
    }

    //-----------------------------------------------------------------------------
    // Queues
    //-----------------------------------------------------------------------------
    bool queue_create(QueueHandle &out_handle, std::size_t length, std::size_t item_size) noexcept
    {
        QueueHandle_t native = xQueueCreate(static_cast<UBaseType_t>(length),
                                            static_cast<UBaseType_t>(item_size));
        if (!native)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<QueueHandle>(native);
        return true;
    }

    void queue_delete(QueueHandle handle) noexcept
    {
        if (handle)
        {
            vQueueDelete(static_cast<QueueHandle_t>(handle));
        }
    }

    bool queue_send(QueueHandle handle, const void *item, Millis timeout_ms) noexcept
    {
        return xQueueSend(static_cast<QueueHandle_t>(handle),
                          item,
                          to_ticks(timeout_ms)) == pdTRUE;
    }

    bool queue_receive(QueueHandle handle, void *out_item, Millis timeout_ms) noexcept
    {
        return xQueueReceive(static_cast<QueueHandle_t>(handle),
                             out_item,
                             to_ticks(timeout_ms)) == pdTRUE;
    }

    bool queue_send_isr(QueueHandle handle, const void *item, bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        BaseType_t ok = xQueueSendFromISR(static_cast<QueueHandle_t>(handle),
                                          item,
                                          &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return ok == pdTRUE;
    }

    bool queue_receive_isr(QueueHandle handle, void *out_item, bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        BaseType_t ok = xQueueReceiveFromISR(static_cast<QueueHandle_t>(handle),
                                             out_item,
                                             &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return ok == pdTRUE;
    }

    std::size_t queue_count(QueueHandle handle) noexcept
    {
        return static_cast<std::size_t>(uxQueueMessagesWaiting(static_cast<QueueHandle_t>(handle)));
    }

    std::size_t queue_spaces(QueueHandle handle) noexcept
    {
        return static_cast<std::size_t>(uxQueueSpacesAvailable(static_cast<QueueHandle_t>(handle)));
    }

    bool queue_reset(QueueHandle handle) noexcept
    {
        return xQueueReset(static_cast<QueueHandle_t>(handle)) == pdPASS;
    }

    //-----------------------------------------------------------------------------
    // Message Buffer
    //-----------------------------------------------------------------------------
    bool msgbuf_create(MsgBufferHandle &out_handle, std::size_t capacity_bytes) noexcept
    {
        MessageBufferHandle_t h = xMessageBufferCreate(capacity_bytes);
        if (!h)
        {
            out_handle = nullptr;
            return false;
        }
        out_handle = static_cast<MsgBufferHandle>(h);
        return true;
    }

    void msgbuf_delete(MsgBufferHandle handle) noexcept
    {
        if (handle)
            vMessageBufferDelete(static_cast<MessageBufferHandle_t>(handle));
    }

    std::size_t msgbuf_send(MsgBufferHandle handle,
                            const void *data, std::size_t bytes,
                            Millis timeout_ms) noexcept
    {
        return xMessageBufferSend(static_cast<MessageBufferHandle_t>(handle),
                                  data, bytes, to_ticks(timeout_ms));
    }

    std::size_t msgbuf_receive(MsgBufferHandle handle,
                               void *out, std::size_t max_bytes,
                               Millis timeout_ms) noexcept
    {
        return xMessageBufferReceive(static_cast<MessageBufferHandle_t>(handle),
                                     out, max_bytes, to_ticks(timeout_ms));
    }

    std::size_t msgbuf_send_isr(MsgBufferHandle handle,
                                const void *data, std::size_t bytes,
                                bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        std::size_t sent = xMessageBufferSendFromISR(static_cast<MessageBufferHandle_t>(handle),
                                                     data, bytes, &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return sent;
    }

    std::size_t msgbuf_receive_isr(MsgBufferHandle handle,
                                   void *out, std::size_t max_bytes,
                                   bool *hp_task_woken) noexcept
    {
        BaseType_t higher = pdFALSE;
        std::size_t recvd = xMessageBufferReceiveFromISR(static_cast<MessageBufferHandle_t>(handle),
                                                         out, max_bytes, &higher);
        if (hp_task_woken)
            *hp_task_woken = (higher == pdTRUE);
        return recvd;
    }

    std::size_t msgbuf_next_len(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferNextLengthBytes(static_cast<MessageBufferHandle_t>(handle));
    }

    std::size_t msgbuf_space_available(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferSpaceAvailable(static_cast<MessageBufferHandle_t>(handle));
    }

    bool msgbuf_reset(MsgBufferHandle handle) noexcept
    {
        return xMessageBufferReset(static_cast<MessageBufferHandle_t>(handle)) == pdPASS;
    }

    //-----------------------------------------------------------------------------
    // Asserts
    //-----------------------------------------------------------------------------
    [[noreturn]] void assert_fail(const char * /*expr*/,
                                  const char * /*file*/,
                                  int /*line*/,
                                  const char * /*func*/) noexcept
    {
        // If you want file/line logging, add your logger/ITM/SEGGER print here.
        // Then route to FreeRTOS's assert path:
        configASSERT(0);
        // configASSERT is noreturn in practice; just in case:
        while (true)
        {
            taskYIELD();
        }
    }
} // namespace rtos::backend
