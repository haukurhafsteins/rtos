#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include <zephyr/fs/nvs.h>

#include "zephyr_nvs_test.hpp"

extern const device rtos_test_flash_device{};

namespace
{
std::map<uint16_t, std::vector<std::byte>> records;
std::map<uint16_t, std::size_t> writeCounts;
uint16_t failedWriteId = UINT16_MAX;
}

void zephyr_nvs_test::reset()
{
    records.clear();
    writeCounts.clear();
    failedWriteId = UINT16_MAX;
}

std::size_t zephyr_nvs_test::writeCount(uint16_t id)
{
    return writeCounts[id];
}

void zephyr_nvs_test::failNextWrite(uint16_t id)
{
    failedWriteId = id;
}

int nvs_mount(nvs_fs*)
{
    return 0;
}

ssize_t nvs_read(nvs_fs*, uint16_t id, void* data, std::size_t length)
{
    const auto record = records.find(id);
    if (record == records.end())
        return -ENOENT;
    if (length < record->second.size())
        return -ENOSPC;
    std::memcpy(data, record->second.data(), record->second.size());
    return static_cast<ssize_t>(record->second.size());
}

ssize_t nvs_write(nvs_fs*, uint16_t id, const void* data, std::size_t length)
{
    ++writeCounts[id];
    if (failedWriteId == id)
    {
        failedWriteId = UINT16_MAX;
        return -EIO;
    }
    const auto* begin = static_cast<const std::byte*>(data);
    records[id] = std::vector<std::byte>(begin, begin + length);
    return static_cast<ssize_t>(length);
}

int nvs_delete(nvs_fs*, uint16_t id)
{
    return records.erase(id) == 0 ? -ENOENT : 0;
}

int nvs_clear(nvs_fs*)
{
    records.clear();
    return 0;
}
