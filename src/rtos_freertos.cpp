#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"
}

namespace rtos
{
    //-------------------------------------------------------------------------
    // RtosMessageBuffer
    //-------------------------------------------------------------------------
    RtosMessageBuffer(size_t bufferSize)
    {
        _bufferSize = bufferSize;
        _handle = xMessageBufferCreate(bufferSize);
        configASSERT(_handle != nullptr);
    }
    ~RtosMessageBuffer()
    {
        if (_handle != nullptr)
        {
            vMessageBufferDelete(_handle);
            _handle = nullptr;
        }
    }

    bool RtosMessageBuffer::send(const void *data, size_t size, TickType_t timeout = portMAX_DELAY)
    {
        size_t ret = xMessageBufferSend(_handle, data, size, pdMS_TO_TICKS(timeout));
        if (ret != size)
        {
            printf("RtosMessageBufferImpl::send: failed to send %zu bytes, sent %zu\n", size, ret);
            return false;
        }
        return true;
    }

    size_t RtosMessageBuffer::receive(void *msgBuf, size_t msgBufSize, TickType_t timeout = portMAX_DELAY)
    {
        size_t ret = xMessageBufferReceive(_handle, msgBuf, msgBufSize, pdMS_TO_TICKS(timeout));
        if (ret == 0)
        {
            size_t msgSize = xMessageBufferNextLengthBytes(_handle);
            if (msgSize > 0)
                printf("RtosMessageBufferImpl::receive: failed to receive data, msgSize: %zu, msgBufSize: %zu\n", msgSize, msgBufSize);
        }
        return ret;
    }

    size_t size() const { return _bufferSize; }

    //-------------------------------------------------------------------------
    // RtosTask
    //-------------------------------------------------------------------------
    RtosTask(const char *name, uint32_t stackSize, uint32_t priority, TaskFunction func, void *arg)
    {
        BaseType_t res = xTaskCreate(func, name, stackSize, arg, priority, &_handle);
        configASSERT(res == pdPASS);
    }
    ~RtosTask()
    {
        if (_handle != nullptr)
        {
            vTaskDelete(_handle);
            _handle = nullptr;
        }
    }
    bool RtosTask::start()
    {
        bool expected = false;
        if (!started_.compare_exchange_strong(expected, true))
            return false;
        backend::ThreadSpec spec{name_, stack_, prio_, this, &RtosTask::threadTrampoline};
        handle_ = backend::thread_create(spec);
        if (use_gate_)
            backend::thread_start_gate_open(handle_);
        return true;
    }
    backend::ThreadHandle* RtosTask::handle() const { return handle_; }


    //-------------------------------------------------------------------------
    // RtosQueue
    //-------------------------------------------------------------------------
    // QueueHandle_t _handle;
    RtosQueue(size_t length) {
        _handle = xQueueCreate(length, sizeof(T));
        configASSERT(_handle != nullptr);
    }
    ~RtosQueue() {
        if (_handle != nullptr) {
            vQueueDelete(_handle);
            _handle = nullptr;
        }
    }

    bool RtosQueue::send(const T& msg, TickType_t timeout = 0) {
        return xQueueSend(_handle, &msg, timeout) == pdTRUE;
    }

    bool RtosQueue::receive(T& msg, TickType_t timeout = portMAX_DELAY) {
        return xQueueReceive(_handle, &msg, timeout) == pdTRUE;
    }

} // namespace rtos
