#pragma once
#include <cstddef>

namespace rtos::memory
{
    /// The heap region a statistic is asked about. A backend maps each region onto
    /// whatever its allocator can tell apart; regions it does not have report zeros.
    enum class Region
    {
        Internal, ///< on-chip RAM: what RTOS objects and driver descriptors must come from
        External, ///< off-chip RAM (PSRAM); zeros when the target has none
        Any,      ///< whatever plain malloc() draws from
        Dma,      ///< the part of Internal that peripheral DMA can address
    };

    /// One snapshot of one region. A field the backend cannot report is 0.
    struct HeapStats
    {
        std::size_t free_bytes = 0;
        std::size_t largest_free_block = 0;
        std::size_t minimum_free_bytes = 0; ///< low-water mark of free_bytes since boot
    };

    /// Read the current statistics of one heap region. Safe to call from any task;
    /// not from an ISR. See memory.md for what each backend puts behind the regions.
    HeapStats heap_stats(Region region);
}
