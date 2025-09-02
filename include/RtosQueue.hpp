#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>
#include "backend.hpp"

template <typename T>
class RtosQueue {
public:
    explicit RtosQueue(std::size_t length) noexcept
        : _handle(nullptr), _length(length)
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "RtosQueue<T> requires T to be trivially copyable. "
                      "For complex types, queue a pointer or a POD wrapper.");
        (void)rtos::backend::queue_create(_handle, length, sizeof(T));
        // If you prefer hard-fail:
        // bool ok = rtos::backend::queue_create(_handle, length, sizeof(T));
        // assert(ok && "queue_create failed");
    }

    ~RtosQueue() { close(); }

    // Non-copyable (owns a native handle)
    RtosQueue(const RtosQueue&) = delete;
    RtosQueue& operator=(const RtosQueue&) = delete;

    // Moveable
    RtosQueue(RtosQueue&& other) noexcept { move_from(std::move(other)); }
    RtosQueue& operator=(RtosQueue&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    // Blocking send/recv with millisecond timeouts
    bool send(const T& msg, uint32_t timeout_ms = 0) noexcept {
        return rtos::backend::queue_send(_handle, &msg, timeout_ms);
    }

    bool receive(T& msg, uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept {
        return rtos::backend::queue_receive(_handle, &msg, timeout_ms);
    }

    // Non-blocking convenience
    bool try_send(const T& msg) noexcept { return send(msg, 0); }
    bool try_receive(T& msg) noexcept { return receive(msg, 0); }

    // ISR variants
    bool send_isr(const T& msg, bool* hp_task_woken = nullptr) noexcept {
        return rtos::backend::queue_send_isr(_handle, &msg, hp_task_woken);
    }
    bool receive_isr(T& msg, bool* hp_task_woken = nullptr) noexcept {
        return rtos::backend::queue_receive_isr(_handle, &msg, hp_task_woken);
    }

    // Introspection / maintenance
    std::size_t count() const noexcept { return rtos::backend::queue_count(_handle); }
    std::size_t spaces() const noexcept { return rtos::backend::queue_spaces(_handle); }
    std::size_t length() const noexcept { return _length; }
    bool reset() noexcept { return rtos::backend::queue_reset(_handle); }

private:
    void close() noexcept {
        if (_handle) {
            rtos::backend::queue_delete(_handle);
            _handle = nullptr;
        }
    }

    void move_from(RtosQueue&& other) noexcept {
        _handle = other._handle;
        _length = other._length;
        other._handle = nullptr;
        other._length = 0;
    }

    rtos::backend::QueueHandle _handle;
    std::size_t _length;
};
