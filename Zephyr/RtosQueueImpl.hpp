// Zephyr/RtosQueueImpl.hpp
#pragma once

#include <zephyr/kernel.h>

template<typename T>
class RtosQueueImpl {
public:
    RtosQueueImpl(size_t length) : _length(length) {
        k_msgq_init(&_msgq, _buffer, sizeof(T), length);
    }

    bool send(const T& msg, k_timeout_t timeout = K_NO_WAIT) {
        return k_msgq_put(&_msgq, &msg, timeout) == 0;
    }

    bool receive(T& msg, k_timeout_t timeout = K_FOREVER) {
        return k_msgq_get(&_msgq, &msg, timeout) == 0;
    }

private:
    static constexpr size_t ALIGN = 4;
    size_t _length;
    alignas(ALIGN) char _buffer[sizeof(T) * 10]; // Use dynamic allocation for real use
    struct k_msgq _msgq;
};
