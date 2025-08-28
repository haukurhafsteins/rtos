#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"

#if defined(USE_FREERTOS)
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"
}

namespace rtos
{
    //-------------------------------------------------------------------------
    // RtosMsgBuffer
    //-------------------------------------------------------------------------
    RtosMsgBuffer(size_t bufferSize)
    {
        _bufferSize = bufferSize;
        _handle = xMessageBufferCreate(bufferSize);
        configASSERT(_handle != nullptr);
    }
    ~RtosMsgBuffer()
    {
        if (_handle != nullptr)
        {
            vMessageBufferDelete(_handle);
            _handle = nullptr;
        }
    }

    bool RtosMsgBuffer::send(const void *data, size_t size, TickType_t timeout = portMAX_DELAY)
    {
        size_t ret = xMessageBufferSend(_handle, data, size, pdMS_TO_TICKS(timeout));
        if (ret != size)
        {
            printf("RtosMessageBufferImpl::send: failed to send %zu bytes, sent %zu\n", size, ret);
            return false;
        }
        return true;
    }

    size_t RtosMsgBuffer::receive(void *msgBuf, size_t msgBufSize, TickType_t timeout = portMAX_DELAY)
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
    ~RtosTask()
    {
        if (handle_ != nullptr)
        {
            vTaskDelete(handle_);
            handle_ = nullptr;
        }
    }
    bool RtosTask::start()
    {
        if (started_)
            return false;
        BaseType_t res = xTaskCreate(func_, name_, stackSize_, arg_, priority_, &handle_);
        configASSERT(res == pdPASS);
        return true;
    }
    bool RtosTask::isStarted() const
    {
        return started_;
    }

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

#endif // USE_FREERTOS
