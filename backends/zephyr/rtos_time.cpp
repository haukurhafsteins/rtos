#include "rtos/time.hpp"

#include <limits>

#include <zephyr/kernel.h>

namespace
{
std::uint64_t nowMicroseconds() noexcept
{
    return k_ticks_to_us_floor64(k_uptime_ticks());
}
}

namespace rtos::time
{
HighResClock::time_point HighResClock::now() noexcept
{
    return time_point(Micros(nowMicroseconds()));
}

Millis now_us() noexcept
{
    return Millis(nowMicroseconds());
}

Millis now_ms() noexcept
{
    return Millis(nowMicroseconds() / 1000U);
}

Seconds now_s() noexcept
{
    return Seconds(nowMicroseconds() / 1000000U);
}

void sleep_for(Millis duration) noexcept
{
    k_sleep(K_MSEC(duration.count()));
}

void sleep_until(HighResClock::time_point deadline) noexcept
{
    const auto now = HighResClock::now();
    if (deadline <= now)
        return;

    auto remaining = std::chrono::duration_cast<Micros>(deadline - now).count();
    while (remaining > 0)
    {
        const auto chunk = remaining > std::numeric_limits<std::int32_t>::max()
            ? std::numeric_limits<std::int32_t>::max()
            : static_cast<std::int32_t>(remaining);
        k_usleep(chunk);
        remaining = std::chrono::duration_cast<Micros>(deadline - HighResClock::now()).count();
    }
}
}
