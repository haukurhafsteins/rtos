#pragma once
#include <utility>

namespace rtos::config
{
    struct Error
    {
        const char *where{nullptr}; // e.g. "mqtt.broker.url"
        const char *msg{nullptr};   // e.g. "missing required"
    };

    template <class T>
    struct Result
    {
        T value{};
        Error error{};
        explicit operator bool() const { return error.where == nullptr; }
        static Result ok(T v) { return Result{std::move(v), {}}; }
        static Result fail(const char *where, const char *msg) { return Result{{}, {where, msg}}; }
    };

    template <>
    struct Result<void>
    {
        Error error{};
        explicit operator bool() const { return error.where == nullptr; }
        static Result ok() { return Result{}; }
        static Result fail(const char *where, const char *msg) { return Result{{where, msg}}; }
    };
} // namespace rtos::config