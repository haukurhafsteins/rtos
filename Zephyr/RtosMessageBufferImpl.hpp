// Zephyr/RtosMessageBufferImpl.hpp
#pragma once

#include <zephyr/kernel.h>
#include <cstring>

class RtosMessageBufferImpl {
public:
    RtosMessageBufferImpl(size_t bufferSize) : _size(bufferSize) {
        k_msgq_init(&_msgq, _buffer, 1, bufferSize); // byte-wise
    }

    bool send(const void* data, size_t size, k_timeout_t timeout = K_MSEC(100)) {
        const uint8_t* byteData = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            if (k_msgq_put(&_msgq, &byteData[i], timeout) != 0) {
                return false;
            }
        }
        return true;
    }

    size_t receive(void* buffer, size_t maxSize, k_timeout_t timeout = K_FOREVER) {
        uint8_t* byteBuf = static_cast<uint8_t*>(buffer);
        size_t count = 0;
        while (count < maxSize) {
            if (k_msgq_get(&_msgq, &byteBuf[count], timeout) != 0) {
                break;
            }
            ++count;
        }
        return count;
    }

private:
    size_t _size;
    alignas(4) uint8_t _buffer[256];  // static allocation for now
    struct k_msgq _msgq;
};
