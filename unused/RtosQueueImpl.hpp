#pragma once

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
}

template<typename T>
class RtosQueue {
public:
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

    bool send(const T& msg, TickType_t timeout = 0) {
        return xQueueSend(_handle, &msg, timeout) == pdTRUE;
    }

    bool receive(T& msg, TickType_t timeout = portMAX_DELAY) {
        return xQueueReceive(_handle, &msg, timeout) == pdTRUE;
    }

private:
    QueueHandle_t _handle;
};
