#pragma once
#include <cstddef>
#include <cstdint>

namespace rtos::zephyr_diagnostics {
enum class Failure : unsigned { TaskSlot, TaskCreate, Queue, MessageBuffer, Pipeline, Count };
struct Failures { std::uint32_t counts[static_cast<unsigned>(Failure::Count)]{}; };
// Enable once from main, before application construction. No reset of history.
void begin() noexcept;
Failures failures() noexcept;
#if defined(CONFIG_RTOS_DIAGNOSTICS)
void record_failure(Failure failure) noexcept;
#else
inline void record_failure(Failure) noexcept {}
#endif
struct StackSnapshot {
    char name[32]{};
    std::size_t minimumFreeBytes = 0;
    int error = 0;
    bool active = false;
};
std::size_t task_slot_count() noexcept;
// Thread context only. Copy one initialized slot's stack-fill watermark;
// no callback/logging under the backend locks. Single-CPU diagnostic feature.
bool task_stack_snapshot(std::size_t index, StackSnapshot &snapshot) noexcept;
}
