#include <gtest/gtest.h>

#include "rtos/memory.hpp"

using rtos::memory::HeapStats;
using rtos::memory::Region;

namespace
{
// The invariants every backend must keep, whatever it can or cannot report:
// no free block is larger than all free bytes together, and the low-water mark
// of free bytes can never sit above the current figure.
void expectConsistent(const HeapStats& stats)
{
    EXPECT_GE(stats.free_bytes, stats.largest_free_block);
    EXPECT_LE(stats.minimum_free_bytes, stats.free_bytes);
}
}

TEST(MemoryStatsTest, AnyReportsAConsistentSnapshot)
{
    expectConsistent(rtos::memory::heap_stats(Region::Any));
}

TEST(MemoryStatsTest, EveryRegionAnswersWithoutFailing)
{
    for (const Region region : {Region::Internal, Region::External, Region::Any, Region::Dma})
        expectConsistent(rtos::memory::heap_stats(region));
}

TEST(MemoryStatsTest, ExternalIsZerosOnTheHost)
{
    const HeapStats external = rtos::memory::heap_stats(Region::External);
    EXPECT_EQ(external.free_bytes, 0u);
    EXPECT_EQ(external.largest_free_block, 0u);
    EXPECT_EQ(external.minimum_free_bytes, 0u);
}

TEST(MemoryStatsTest, InternalAnyAndDmaDescribeTheHostsSingleHeap)
{
    // Nothing allocates between the reads, so the three views of the one process
    // heap must agree exactly (and are all zeros where the C library reports nothing).
    const HeapStats internal = rtos::memory::heap_stats(Region::Internal);
    const HeapStats any = rtos::memory::heap_stats(Region::Any);
    const HeapStats dma = rtos::memory::heap_stats(Region::Dma);
    EXPECT_EQ(internal.free_bytes, any.free_bytes);
    EXPECT_EQ(internal.free_bytes, dma.free_bytes);
    EXPECT_EQ(internal.largest_free_block, any.largest_free_block);
    EXPECT_EQ(internal.minimum_free_bytes, any.minimum_free_bytes);
}
