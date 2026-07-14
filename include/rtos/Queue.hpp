#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>
#include "rtos/backend.hpp"

namespace rtos
{

template <typename T>
class Queue {
public:
    explicit Queue(std::size_t length) noexcept
        : _handle(nullptr), _length(length)
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Queue<T> requires T to be trivially copyable. "
                      "For complex types, queue a pointer or a POD wrapper.");
        (void)backend::queue_create(_handle, length, sizeof(T));
        // If you prefer hard-fail:
        // bool ok = backend::queue_create(_handle, length, sizeof(T));
        // assert(ok && "queue_create failed");
    }

    ~Queue() { close(); }

    // Non-copyable (owns a native handle)
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    // Moveable
    Queue(Queue&& other) noexcept { move_from(std::move(other)); }
    Queue& operator=(Queue&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    // Blocking send/recv with millisecond timeouts
    bool send(const T& msg, uint32_t timeout_ms = 0) noexcept {
        return backend::queue_send(_handle, &msg, timeout_ms);
    }

    bool receive(T& msg, uint32_t timeout_ms = backend::WAIT_FOREVER) noexcept {
        return backend::queue_receive(_handle, &msg, timeout_ms);
    }

    // Non-blocking convenience
    bool try_send(const T& msg) noexcept { return send(msg, 0); }
    bool try_receive(T& msg) noexcept { return receive(msg, 0); }

    // ISR variants
    bool send_isr(const T& msg, bool* hp_task_woken = nullptr) noexcept {
        return backend::queue_send_isr(_handle, &msg, hp_task_woken);
    }
    bool receive_isr(T& msg, bool* hp_task_woken = nullptr) noexcept {
        return backend::queue_receive_isr(_handle, &msg, hp_task_woken);
    }

    // Introspection / maintenance
    std::size_t count() const noexcept { return backend::queue_count(_handle); }
    std::size_t spaces() const noexcept { return backend::queue_spaces(_handle); }
    std::size_t length() const noexcept { return _length; }
    bool reset() noexcept { return backend::queue_reset(_handle); }

private:
    void close() noexcept {
        if (_handle) {
            backend::queue_delete(_handle);
            _handle = nullptr;
        }
    }

    void move_from(Queue&& other) noexcept {
        _handle = other._handle;
        _length = other._length;
        other._handle = nullptr;
        other._length = 0;
    }

    backend::QueueHandle _handle;
    std::size_t _length;
};

} // namespace rtos
