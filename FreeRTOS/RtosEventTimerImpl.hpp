// FreeRTOS/RtosEventTimerImpl.hpp
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <functional>

class RtosEventTimerImpl {
public:
    using Callback = std::function<void()>;

    RtosEventTimerImpl(const char* name, uint32_t periodMs, bool periodic, Callback cb)
        : _callback(cb) {
        _timer = xTimerCreate(
            name,
            pdMS_TO_TICKS(periodMs),
            periodic ? pdTRUE : pdFALSE,
            this,
            [](TimerHandle_t xTimer) {
                auto* self = static_cast<RtosEventTimerImpl*>(pvTimerGetTimerID(xTimer));
                if (self && self->_callback) self->_callback();
            });
        configASSERT(_timer != nullptr);
    }

    void start() {
        xTimerStart(_timer, 0);
    }

    void stop() {
        xTimerStop(_timer, 0);
    }

    ~RtosEventTimerImpl() {
        xTimerDelete(_timer, 0);
    }

private:
    TimerHandle_t _timer;
    Callback _callback;
};
