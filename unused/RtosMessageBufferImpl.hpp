// FreeRTOS/RtosMessageBufferImpl.hpp
#pragma once
#include <cstdint>
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
}

class RtosMessageBufferImpl {
public:
    RtosMessageBufferImpl(size_t bufferSize) {
        _bufferSize = bufferSize;
        _handle = xMessageBufferCreate(bufferSize);
        configASSERT(_handle != nullptr);
    }
    ~RtosMessageBufferImpl() {
        if (_handle != nullptr) {
            vMessageBufferDelete(_handle);
            _handle = nullptr;
        }
    }

    bool send(const void* data, size_t size, TickType_t timeout = portMAX_DELAY) {
        size_t ret = xMessageBufferSend(_handle, data, size, pdMS_TO_TICKS(timeout));
        if (ret != size) {
            printf("RtosMessageBufferImpl::send: failed to send %zu bytes, sent %zu\n", size, ret);
            return false;
        }
        return true;
    }

    size_t receive(void* msgBuf, size_t msgBufSize, TickType_t timeout = portMAX_DELAY) {
        size_t ret = xMessageBufferReceive(_handle, msgBuf, msgBufSize, pdMS_TO_TICKS(timeout));
        if (ret == 0) {
            size_t msgSize = xMessageBufferNextLengthBytes(_handle);
            if (msgSize > 0)
                printf("RtosMessageBufferImpl::receive: failed to receive data, msgSize: %zu, msgBufSize: %zu\n", msgSize, msgBufSize);
        }
        return ret;
    }

    size_t size() const { return _bufferSize; }

private:
    MessageBufferHandle_t _handle;
    size_t _bufferSize;
};
