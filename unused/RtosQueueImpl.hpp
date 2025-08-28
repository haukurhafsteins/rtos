// FreeRTOS/RtosQueueImpl.hpp
#pragma once

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
}

template<typename T>
class RtosQueueImpl {
public:
    RtosQueueImpl(size_t length) {
        _handle = xQueueCreate(length, sizeof(T));
        configASSERT(_handle != nullptr);
    }
    ~RtosQueueImpl() {
        if (_handle != nullptr) {
            vQueueDelete(_handle);
            _handle = nullptr;
        }
    }

    bool send(const T& msg, TickType_t timeout = 0) {
        return xQueueSend(_handle, &msg, timeout) == pdTRUE;
    }

    bool receive(T& msg, TickType_t timeout = portMAX_DELAY) {
        return xQueueReceive(_handle, &msg, timeout) == pdTRUE;
    }

private:
    QueueHandle_t _handle;
};
