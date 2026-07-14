#pragma once
#include <cstdint>
#include <utility>
#include "rtos/backend.hpp"

namespace rtos
{

class Task {
public:
    Task(const char* name,
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
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Moveable
    Task(Task&& other) noexcept { move_from(std::move(other)); }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            stop();
            move_from(std::move(other));
        }
        return *this;
    }

    ~Task() { stop(); }

    /// @brief Start the task on the specified core (or any core if core_id is TASK_NO_AFFINITY)
    /// @param core_id Core ID to pin the task to, or TASK_NO_AFFINITY to allow any core
    /// @return true if the task was started successfully, false if it was already started
    bool start(int core_id = TASK_NO_AFFINITY) noexcept {
        if (_started) return false;
        bool ok = false;
        if (core_id == TASK_NO_AFFINITY)
            ok = backend::task_create(_handle, _name, _stack_size_bytes, _priority, _func, _arg);
        else
            ok = backend::task_create_pinned(_handle, _name, _stack_size_bytes, _priority, core_id, _func, _arg);
        _started = ok;
        return ok;
    }

    // Explicitly delete the task (safe to call multiple times)
    void stop() noexcept {
        if (_handle) {
            backend::task_delete(_handle);
            _handle = nullptr;
        }
        _started = false;
    }

    backend::TaskHandle handle() const noexcept { return _handle; }
    bool started() const noexcept { return _started; }

    // Convenience wrappers
    static void sleep_ms(Millis duration) noexcept { backend::delay_ms(duration); }
    static void yield() noexcept { backend::yield(); }
    /// @brief Get the handle of the currently running task
    /// @return Handle of the currently running task
    static backend::TaskHandle current() noexcept { return backend::current_task(); }
    constexpr static int TASK_NO_AFFINITY = -1;

private:
    void move_from(Task&& other) noexcept {
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

    backend::TaskHandle _handle;
    const char* _name;
    uint32_t _stack_size_bytes;
    uint32_t _priority;
    TaskFunction _func;
    void* _arg;
    bool _started;
};

} // namespace rtos
