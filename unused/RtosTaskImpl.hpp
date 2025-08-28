// FreeRTOS/RtosTaskImpl.hpp
#pragma once

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

using TaskFunction = void (*)(void*);

class RtosTaskImpl {
public:
    RtosTaskImpl(const char* name, uint32_t stackSize, UBaseType_t priority, TaskFunction func, void* arg) {
        BaseType_t res = xTaskCreate(func, name, stackSize, arg, priority, &_handle);
        configASSERT(res == pdPASS);
    }
    ~RtosTaskImpl() {
        if (_handle != nullptr) {
            vTaskDelete(_handle);
            _handle = nullptr;
        }
    }

    TaskHandle_t handle() const { return _handle; }

private:
    TaskHandle_t _handle;
};
