// FreeRTOS/RtosQueueImpl.hpp
#pragma once
#include <cstddef>
#include "backend.hpp"

template<typename T>
class RtosQueueImpl {
public:
    RtosQueueImpl(size_t length);
    ~RtosQueueImpl();

    bool send(const T& msg, TickType_t timeout = 0);
    bool receive(T& msg, TickType_t timeout = portMAX_DELAY);

private:
    QueueHandle _handle;
};
