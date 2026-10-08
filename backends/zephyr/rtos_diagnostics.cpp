#include "rtos/ZephyrDiagnostics.hpp"
#include <atomic>
#include <limits>

namespace rtos::zephyr_diagnostics {
namespace {
std::atomic<bool> enabled{false};
std::atomic<std::uint32_t> counts[static_cast<unsigned>(Failure::Count)]{};
}
void begin() noexcept { enabled.store(true, std::memory_order_release); }
void record_failure(Failure failure) noexcept
{
    if (!enabled.load(std::memory_order_acquire)) return;
    const auto index = static_cast<unsigned>(failure);
    if (index >= static_cast<unsigned>(Failure::Count)) return;
    auto &counter = counts[index];
    auto value = counter.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint32_t>::max() &&
           !counter.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) {}
}
Failures failures() noexcept
{
    Failures result;
    for (unsigned i = 0; i < static_cast<unsigned>(Failure::Count); ++i)
        result.counts[i] = counts[i].load(std::memory_order_relaxed);
    return result;
}
}
