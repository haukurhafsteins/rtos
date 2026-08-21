#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace zephyr_memory_appinfo_test
{
void resetMemory();
std::size_t allocatedBytes();
bool allAllocationsWereNoWait();

void setImageVersion(
    uint8_t major,
    uint8_t minor,
    uint16_t revision,
    uint32_t buildNumber);
uint8_t lastImageAreaId();

void setDeviceId(const std::array<uint8_t, 8>& id);
void setDeviceIdResult(ssize_t result);
}
