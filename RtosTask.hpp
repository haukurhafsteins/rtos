#pragma once
#include <cstdint>
#include <utility>
#include "backend.hpp"

class RtosTask {
public:
    RtosTask(const char* name,
             uint32_t stack_size_bytes,
             uint32_t priority,
             TaskFunction func,
             void* arg) noexcept
    : _handle(nullptr),
      _name(name),
      _stack_size_bytes(stack_size_bytes),
      _priority(priority),
      _func(func),
      _arg(arg),
      _started(false) {}

    // Non-copyable (we own a native handle)
    RtosTask(const RtosTask&) = delete;
    RtosTask& operator=(const RtosTask&) = delete;

    // Moveable
    RtosTask(RtosTask&& other) noexcept { move_from(std::move(other)); }
    RtosTask& operator=(RtosTask&& other) noexcept {
        if (this != &other) {
            stop();
            move_from(std::move(other));
        }
        return *this;
    }

    ~RtosTask() { stop(); }

    bool start() noexcept {
        if (_started) return false;
        bool ok = rtos::backend::task_create(_handle, _name, _stack_size_bytes, _priority, _func, _arg);
        _started = ok;
        return ok;
    }

    // Explicitly delete the task (safe to call multiple times)
    void stop() noexcept {
        if (_handle) {
            rtos::backend::task_delete(_handle);
            _handle = nullptr;
        }
        _started = false;
    }

    rtos::backend::TaskHandle handle() const noexcept { return _handle; }
    bool started() const noexcept { return _started; }

    // Convenience wrappers
    static void sleep_ms(uint32_t ms) noexcept { rtos::backend::delay_ms(ms); }
    static void yield() noexcept { rtos::backend::yield(); }
    static rtos::backend::TaskHandle current() noexcept { return rtos::backend::current_task(); }

private:
    void move_from(RtosTask&& other) noexcept {
        _handle = other._handle;
        _name = other._name;
        _stack_size_bytes = other._stack_size_bytes;
        _priority = other._priority;
        _func = other._func;
        _arg = other._arg;
        _started = other._started;

        other._handle = nullptr;
        other._started = false;
    }

    rtos::backend::TaskHandle _handle;
    const char* _name;
    uint32_t _stack_size_bytes;
    uint32_t _priority;
    TaskFunction _func;
    void* _arg;
    bool _started;
};
