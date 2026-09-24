#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#ifdef _MSC_VER
#include <BaseTsd.h>
using ssize_t = SSIZE_T; // MSVC has no ssize_t; the Zephyr API these stubs mirror uses it
#else
#include <sys/types.h>
#endif

namespace zephyr_memory_appinfo_test
{
void resetMemory();
std::size_t allocatedBytes();
bool allAllocationsWereNoWait();

/// What sys_heap_runtime_stats_get() reports for the kernel system heap.
void setSystemHeapStats(
    std::size_t freeBytes, std::size_t allocatedBytes, std::size_t maxAllocatedBytes);
/// Return code of sys_heap_runtime_stats_get() for every heap (0 succeeds).
void setHeapStatsResult(int result);

void setImageVersion(
    uint8_t major,
    uint8_t minor,
    uint16_t revision,
    uint32_t buildNumber);
uint8_t lastImageAreaId();

void setDeviceId(const std::array<uint8_t, 8>& id);
void setDeviceIdResult(ssize_t result);
}
