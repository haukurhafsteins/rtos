// time.hpp
#pragma once
#include <chrono>
#include <cstdint>

namespace rtos::time
{

    // High-res monotonic clock (µs since boot)
    struct HighResClock
    {
        using rep = int64_t;
        using period = std::micro;
        using duration = std::chrono::microseconds;
        using time_point = std::chrono::time_point<HighResClock>;
        static constexpr bool is_steady = true;

        // Implemented in backend (FreeRTOS/Zephyr/etc.)
        static time_point now() noexcept;
    };

    // Convenience aliases
    using Micros = std::chrono::microseconds;
    using Millis = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    // Sleep wrappers (map to vTaskDelay/k_sleep/etc.)
    void sleep_for(Millis ms) noexcept;
    void sleep_until(HighResClock::time_point tp) noexcept;
    Millis now_us() noexcept;
    Millis now_ms() noexcept;
    Seconds now_s() noexcept;
    template <typename T = double>
    inline T now_seconds() noexcept
    {
        static_assert(std::is_floating_point_v<T>, "T must be a floating-point type");
        constexpr T us_to_s = static_cast<T>(1e-6);
        return static_cast<T>(now_us().count()) * us_to_s;
    }
} // namespace rtos::time
