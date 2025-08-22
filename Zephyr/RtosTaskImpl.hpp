// Zephyr/RtosTaskImpl.hpp
#pragma once

#include <zephyr/kernel.h>

using TaskFunction = void (*)(void*, void*, void*);

class RtosTaskImpl {
public:
    RtosTaskImpl(const char* name, size_t stackSize, int priority, TaskFunction entry, void* p1 = nullptr)
        : _stackSize(stackSize) {

        k_thread_create(&_threadData, _stack, stackSize,
                        entry, p1, nullptr, nullptr,
                        priority, 0, K_NO_WAIT);
        k_thread_name_set(&_threadData, name);
    }
    ~RtosTaskImpl() {
        k_thread_abort(&_threadData);
    }

private:
    size_t _stackSize;
    struct k_thread _threadData;
    alignas(4) uint8_t _stack[CONFIG_MAIN_STACK_SIZE]; // optionally use stackSize if dynamic is allowed
};
