#include "zephyr_memory_appinfo_test.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>

namespace
{
std::unordered_map<void*, std::size_t> allocations;
bool usedBlockingTimeout = false;
mcuboot_img_sem_ver imageVersion{2, 3, 4, 5};
uint8_t imageAreaId = 0;
std::array<uint8_t, 8> deviceId{0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
ssize_t deviceIdResult = 8;

std::size_t bytesAllocated()
{
    std::size_t total = 0;
    for (const auto& [pointer, size] : allocations)
    {
        (void)pointer;
        total += size;
    }
    return total;
}
}

void* k_heap_alloc(k_heap* heap, std::size_t bytes, k_timeout_t timeout)
{
    usedBlockingTimeout |= timeout.ticks != K_NO_WAIT.ticks;
    if (bytes == 0 || bytesAllocated() + bytes > heap->capacity)
        return nullptr;
    void* pointer = std::malloc(bytes);
    if (pointer != nullptr)
        allocations[pointer] = bytes;
    return pointer;
}

void* k_heap_realloc(
    k_heap* heap, void* pointer, std::size_t bytes, k_timeout_t timeout)
{
    usedBlockingTimeout |= timeout.ticks != K_NO_WAIT.ticks;
    if (pointer == nullptr)
        return k_heap_alloc(heap, bytes, timeout);
    const auto found = allocations.find(pointer);
    if (found == allocations.end())
        return nullptr;
    if (bytes == 0)
    {
        k_heap_free(heap, pointer);
        return nullptr;
    }
    if (bytesAllocated() - found->second + bytes > heap->capacity)
        return nullptr;
    void* resized = std::realloc(pointer, bytes);
    if (resized == nullptr)
        return nullptr;
    allocations.erase(found);
    allocations[resized] = bytes;
    return resized;
}

void k_heap_free(k_heap*, void* pointer)
{
    if (pointer == nullptr)
        return;
    const auto found = allocations.find(pointer);
    if (found == allocations.end())
        return;
    std::free(pointer);
    allocations.erase(found);
}

std::size_t sys_heap_usable_size(sys_heap*, void* pointer)
{
    const auto found = allocations.find(pointer);
    return found == allocations.end() ? 0 : found->second;
}

int boot_read_bank_header(
    uint8_t areaId, mcuboot_img_header* header, std::size_t headerSize)
{
    imageAreaId = areaId;
    if (header == nullptr || headerSize < sizeof(*header))
        return -1;
    header->mcuboot_version = 1;
    header->h.v1.image_size = 1234;
    header->h.v1.sem_ver = imageVersion;
    return 0;
}

ssize_t hwinfo_get_device_id(uint8_t* buffer, std::size_t length)
{
    if (deviceIdResult < 0)
        return deviceIdResult;
    const auto copied = std::min(
        length,
        std::min(deviceId.size(), static_cast<std::size_t>(deviceIdResult)));
    std::memcpy(buffer, deviceId.data(), copied);
    return static_cast<ssize_t>(copied);
}

namespace zephyr_memory_appinfo_test
{
void resetMemory()
{
    for (const auto& [pointer, size] : allocations)
    {
        (void)size;
        std::free(pointer);
    }
    allocations.clear();
    usedBlockingTimeout = false;
}

std::size_t allocatedBytes()
{
    return bytesAllocated();
}

bool allAllocationsWereNoWait()
{
    return !usedBlockingTimeout;
}

void setImageVersion(
    uint8_t major,
    uint8_t minor,
    uint16_t revision,
    uint32_t buildNumber)
{
    imageVersion = {major, minor, revision, buildNumber};
}

uint8_t lastImageAreaId()
{
    return imageAreaId;
}

void setDeviceId(const std::array<uint8_t, 8>& id)
{
    deviceId = id;
    deviceIdResult = static_cast<ssize_t>(id.size());
}

void setDeviceIdResult(ssize_t result)
{
    deviceIdResult = result;
}
}
