#include "rtos/memory.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>

// The kernel system heap that k_malloc() draws from. Zephyr defines it in
// kernel/mempool.c without a public declaration; the kernel shell's `heap`
// command reaches it the same way.
extern "C"
{
    extern struct k_heap _system_heap;
}
#ifdef CONFIG_RTOS_PSRAM
extern struct k_heap rtos_psram_heap; // K_HEAP_DEFINE in rtos_psram.cpp, a C++ unit
#endif

namespace rtos::memory
{
namespace
{
HeapStats statsOf(k_heap& heap)
{
    sys_memory_stats stats{};
    HeapStats result;
    if (sys_heap_runtime_stats_get(&heap.heap, &stats) != 0)
        return result;
    result.free_bytes = stats.free_bytes;
    // The runtime statistics carry no largest-block figure, so that field stays 0.
    // The low-water mark of free bytes is the heap size less the peak allocation.
    const std::size_t total = stats.free_bytes + stats.allocated_bytes;
    result.minimum_free_bytes =
        stats.max_allocated_bytes <= total ? total - stats.max_allocated_bytes : 0;
    return result;
}
}

HeapStats heap_stats(Region region)
{
    switch (region)
    {
    case Region::External:
#ifdef CONFIG_RTOS_PSRAM
        return statsOf(rtos_psram_heap);
#else
        return HeapStats{};
#endif
    case Region::Internal:
    case Region::Any:
    case Region::Dma:
    default:
        return statsOf(_system_heap);
    }
}
}
