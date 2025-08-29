#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>
#include "backend.hpp"

class RtosMsgBuffer {
public:
    explicit RtosMsgBuffer(std::size_t capacity_bytes) noexcept
        : _handle(nullptr), _capacity(capacity_bytes)
    {
        (void)rtos::backend::msgbuf_create(_handle, capacity_bytes);
        // If you prefer hard-fail:
        // bool ok = rtos::backend::msgbuf_create(_handle, capacity_bytes);
        // assert(ok && "msgbuf_create failed");
    }

    ~RtosMsgBuffer() { close(); }

    // Non-copyable
    RtosMsgBuffer(const RtosMsgBuffer&) = delete;
    RtosMsgBuffer& operator=(const RtosMsgBuffer&) = delete;

    // Moveable
    RtosMsgBuffer(RtosMsgBuffer&& other) noexcept { move_from(std::move(other)); }
    RtosMsgBuffer& operator=(RtosMsgBuffer&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    // Raw byte API (returns bytes sent/received)
    std::size_t send(const void* data, std::size_t bytes,
                     uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept
    {
        return rtos::backend::msgbuf_send(_handle, data, bytes, timeout_ms);
    }

    bool send_all(const void* data, std::size_t bytes,
                  uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept
    {
        return send(data, bytes, timeout_ms) == bytes;
    }

    std::size_t receive(void* out, std::size_t max_bytes,
                        uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept
    {
        return rtos::backend::msgbuf_receive(_handle, out, max_bytes, timeout_ms);
    }

    // Typed convenience (trivially copyable payloads only)
    template <typename T>
    bool send_obj(const T& obj, uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept {
        static_assert(std::is_trivially_copyable<T>::value,
                      "send_obj requires trivially copyable T");
        return send_all(&obj, sizeof(T), timeout_ms);
    }

    template <typename T>
    bool receive_obj(T& out, uint32_t timeout_ms = rtos::backend::RTOS_WAIT_FOREVER) noexcept {
        static_assert(std::is_trivially_copyable<T>::value,
                      "receive_obj requires trivially copyable T");
        return receive(&out, sizeof(T), timeout_ms) == sizeof(T);
    }

    // ISR variants
    std::size_t send_isr(const void* data, std::size_t bytes, bool* hp_task_woken = nullptr) noexcept {
        return rtos::backend::msgbuf_send_isr(_handle, data, bytes, hp_task_woken);
    }
    std::size_t receive_isr(void* out, std::size_t max_bytes, bool* hp_task_woken = nullptr) noexcept {
        return rtos::backend::msgbuf_receive_isr(_handle, out, max_bytes, hp_task_woken);
    }

    // Introspection / maintenance
    std::size_t capacity() const noexcept { return _capacity; }
    std::size_t next_msg_size() const noexcept { return rtos::backend::msgbuf_next_len(_handle); }
    std::size_t space_available() const noexcept { return rtos::backend::msgbuf_space_available(_handle); }
    bool reset() noexcept { return rtos::backend::msgbuf_reset(_handle); }

private:
    void close() noexcept {
        if (_handle) {
            rtos::backend::msgbuf_delete(_handle);
            _handle = nullptr;
        }
    }

    void move_from(RtosMsgBuffer&& other) noexcept {
        _handle = other._handle;
        _capacity = other._capacity;
        other._handle = nullptr;
        other._capacity = 0;
    }

    rtos::backend::MsgBufferHandle _handle;
    std::size_t _capacity;
};
