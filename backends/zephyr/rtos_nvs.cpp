// Zephyr backend for rtos::Nvs / rtos::nvs using the raw NVS file system.

#include "rtos/Nvs.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/__assert.h>

using namespace rtos;

#ifndef RTOS_NVS_PARTITION
#define RTOS_NVS_PARTITION storage_partition
#endif

namespace
{
constexpr uint16_t CollisionTableId = 0;
constexpr uint32_t CollisionTableMagic = UINT32_C(0x52544f53); // "RTOS"
constexpr uint16_t CollisionTableVersion = 1;
constexpr size_t MaxNameLength = 15;
constexpr size_t MaxMappings = 64;

enum class ValueType : uint8_t
{
    String = 1,
    Blob = 2,
    U32 = 3,
    I32 = 4,
};

struct Mapping
{
    uint32_t hash = 0;
    uint32_t valueSize = 0;
    uint16_t id = 0;
    ValueType type = ValueType::Blob;
    uint8_t reserved = 0;
    char nspace[MaxNameLength + 1]{};
    char key[MaxNameLength + 1]{};
};

struct CollisionTable
{
    uint32_t magic = CollisionTableMagic;
    uint16_t version = CollisionTableVersion;
    uint16_t count = 0;
    Mapping mappings[MaxMappings]{};
};

struct Handle
{
    char nspace[MaxNameLength + 1]{};
    Nvs::Mode mode = Nvs::Mode::ReadWrite;
};

struct LockGuard
{
    explicit LockGuard(k_mutex& mutex) : mutex_(mutex)
    {
        k_mutex_lock(&mutex_, K_FOREVER);
    }

    ~LockGuard()
    {
        k_mutex_unlock(&mutex_);
    }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    k_mutex& mutex_;
};

K_MUTEX_DEFINE(storageMutex);
nvs_fs filesystem{};
CollisionTable collisionTable{};
bool initialized = false;

Handle* toHandle(uintptr_t handle)
{
    return reinterpret_cast<Handle*>(handle);
}

bool validName(const char* name)
{
    if (name == nullptr || name[0] == '\0')
        return false;
    size_t length = 0;
    while (length <= MaxNameLength && name[length] != '\0')
        ++length;
    return length <= MaxNameLength;
}

uint32_t hashPath(const char* nspace, const char* key)
{
    uint32_t hash = UINT32_C(2166136261);
    auto add = [&hash](char value) {
        hash ^= static_cast<uint8_t>(value);
        hash *= UINT32_C(16777619);
    };

    for (const char* current = nspace; *current != '\0'; ++current)
        add(*current);
    add('/');
    for (const char* current = key; *current != '\0'; ++current)
        add(*current);
    return hash;
}

uint16_t idForHash(uint32_t hash)
{
    const auto id = static_cast<uint16_t>(
        hash & std::numeric_limits<uint16_t>::max());
    return id == CollisionTableId ? UINT16_C(1) : id;
}

bool mappingTypeValid(ValueType type)
{
    return type == ValueType::String || type == ValueType::Blob ||
        type == ValueType::U32 || type == ValueType::I32;
}

bool assertValid(bool condition, const char* message)
{
    __ASSERT(condition, "%s", message);
    (void)message;
    return condition;
}

size_t collisionTableSize()
{
    return offsetof(CollisionTable, mappings) +
        collisionTable.count * sizeof(collisionTable.mappings[0]);
}

void resetCollisionTable()
{
    // No aggregate assignment here: `collisionTable = CollisionTable{}`
    // materializes a ~2.8 KiB temporary on the caller's stack, which
    // overflowed a 1 KiB main stack and MPU-faulted the nRF5340 on first
    // init (URU bench, 2026-08-20). Reset in place instead.
    std::memset(&collisionTable, 0, sizeof(collisionTable));
    collisionTable.magic = CollisionTableMagic;
    collisionTable.version = CollisionTableVersion;
}

bool validateCollisionTable(size_t storedSize)
{
    if (!assertValid(collisionTable.magic == CollisionTableMagic,
            "rtos NVS collision table has an invalid magic value") ||
        !assertValid(collisionTable.version == CollisionTableVersion,
            "rtos NVS collision table has an unsupported version") ||
        !assertValid(collisionTable.count <= MaxMappings,
            "rtos NVS collision table is too large") ||
        !assertValid(storedSize == collisionTableSize(),
            "rtos NVS collision table has an invalid size"))
        return false;

    for (size_t index = 0; index < collisionTable.count; ++index)
    {
        const auto& current = collisionTable.mappings[index];
        if (!assertValid(validName(current.nspace) && validName(current.key),
                "rtos NVS collision table contains an invalid name") ||
            !assertValid(mappingTypeValid(current.type),
                "rtos NVS collision table contains an invalid type") ||
            !assertValid(current.hash == hashPath(current.nspace, current.key),
                "rtos NVS collision table contains an invalid hash") ||
            !assertValid(current.id == idForHash(current.hash),
                "rtos NVS collision table contains an invalid id"))
            return false;

        for (size_t otherIndex = index + 1;
             otherIndex < collisionTable.count;
             ++otherIndex)
        {
            const auto& other = collisionTable.mappings[otherIndex];
            if (!assertValid(current.id != other.id,
                    "rtos NVS string-key hash collision"))
                return false;
        }
    }
    return true;
}

bool loadCollisionTable()
{
    resetCollisionTable();
    const ssize_t result = nvs_read(
        &filesystem,
        CollisionTableId,
        &collisionTable,
        sizeof(collisionTable));
    if (result == -ENOENT)
        return true;
    if (result < 0 || static_cast<size_t>(result) > sizeof(collisionTable))
        return false;
    return validateCollisionTable(static_cast<size_t>(result));
}

bool persistCollisionTable()
{
    const size_t size = collisionTableSize();
    const ssize_t result = nvs_write(
        &filesystem,
        CollisionTableId,
        &collisionTable,
        size);
    return result == 0 || result == static_cast<ssize_t>(size);
}

Mapping* findMapping(const Handle& handle, const char* key)
{
    const uint32_t hash = hashPath(handle.nspace, key);
    const uint16_t id = idForHash(hash);

    for (size_t index = 0; index < collisionTable.count; ++index)
    {
        auto& current = collisionTable.mappings[index];
        const bool samePath = current.hash == hash &&
            std::strcmp(current.nspace, handle.nspace) == 0 &&
            std::strcmp(current.key, key) == 0;
        if (samePath)
            return &current;
        if (current.id == id)
        {
            assertValid(false, "rtos NVS string-key hash collision");
            return nullptr;
        }
    }
    return nullptr;
}

Mapping* addMapping(const Handle& handle, const char* key)
{
    if (!assertValid(collisionTable.count < MaxMappings,
            "rtos NVS collision table capacity exceeded"))
        return nullptr;

    const uint32_t hash = hashPath(handle.nspace, key);
    const uint16_t id = idForHash(hash);
    for (size_t index = 0; index < collisionTable.count; ++index)
    {
        if (collisionTable.mappings[index].id == id)
        {
            assertValid(false, "rtos NVS string-key hash collision");
            return nullptr;
        }
    }

    auto& mapping = collisionTable.mappings[collisionTable.count++];
    mapping.hash = hash;
    mapping.id = id;
    std::memcpy(mapping.nspace, handle.nspace, std::strlen(handle.nspace) + 1);
    std::memcpy(mapping.key, key, std::strlen(key) + 1);
    return &mapping;
}

bool readValue(
    const Handle& handle,
    const char* key,
    ValueType type,
    void* output,
    size_t& inoutSize)
{
    if (!validName(key))
        return false;

    auto* mapping = findMapping(handle, key);
    if (mapping == nullptr || mapping->type != type)
        return false;

    const size_t required = mapping->valueSize;
    if (output == nullptr)
    {
        inoutSize = required;
        return true;
    }
    if (inoutSize < required)
    {
        inoutSize = required;
        return false;
    }
    if (required == 0)
    {
        inoutSize = 0;
        return true;
    }

    const ssize_t result = nvs_read(&filesystem, mapping->id, output, required);
    if (result != static_cast<ssize_t>(required))
        return false;
    inoutSize = required;
    return true;
}

bool writeValue(
    const Handle& handle,
    const char* key,
    ValueType type,
    const void* data,
    size_t size)
{
    if (!validName(key) || (data == nullptr && size != 0))
        return false;

    Mapping* mapping = findMapping(handle, key);
    const bool added = mapping == nullptr;
    if (added)
    {
        mapping = addMapping(handle, key);
        if (mapping == nullptr)
            return false;
    }

    const uint8_t emptyValue = 0;
    const void* storedData = size == 0 ? &emptyValue : data;
    const size_t storedSize = size == 0 ? sizeof(emptyValue) : size;
    const ssize_t writeResult = nvs_write(
        &filesystem,
        mapping->id,
        storedData,
        storedSize);
    if (writeResult != 0 && writeResult != static_cast<ssize_t>(storedSize))
    {
        if (added)
        {
            --collisionTable.count;
            *mapping = Mapping{};
        }
        return false;
    }

    const ValueType previousType = mapping->type;
    const uint32_t previousSize = mapping->valueSize;
    const bool metadataChanged = added || previousType != type ||
        previousSize != size;
    if (!metadataChanged)
        return true;

    mapping->type = type;
    mapping->valueSize = static_cast<uint32_t>(size);
    if (persistCollisionTable())
        return true;

    mapping->type = previousType;
    mapping->valueSize = previousSize;
    if (added)
    {
        --collisionTable.count;
        *mapping = Mapping{};
    }
    return false;
}
} // namespace

bool rtos::nvs::init()
{
    LockGuard lock(storageMutex);
    if (initialized)
        return true;

    filesystem.flash_device = FIXED_PARTITION_DEVICE(RTOS_NVS_PARTITION);
    filesystem.offset = FIXED_PARTITION_OFFSET(RTOS_NVS_PARTITION);
    if (!device_is_ready(filesystem.flash_device))
        return false;

    flash_pages_info pageInfo{};
    if (flash_get_page_info_by_offs(
            filesystem.flash_device,
            filesystem.offset,
            &pageInfo) != 0)
        return false;

    const size_t partitionSize = FIXED_PARTITION_SIZE(RTOS_NVS_PARTITION);
    const size_t totalSectorCount = partitionSize / pageInfo.size;
    // Reserve the first half of `storage` for rtos NVS. BLE settings owns the
    // second half, so neither subsystem can erase or garbage-collect the other.
    const size_t rtosSectorCount = totalSectorCount / 2;
    if (pageInfo.size > std::numeric_limits<uint16_t>::max() ||
        rtosSectorCount < 2 ||
        rtosSectorCount > std::numeric_limits<uint16_t>::max() ||
        partitionSize % pageInfo.size != 0)
        return false;

    filesystem.sector_size = static_cast<uint16_t>(pageInfo.size);
    filesystem.sector_count = static_cast<uint16_t>(rtosSectorCount);
    if (nvs_mount(&filesystem) != 0 || !loadCollisionTable())
        return false;

    initialized = true;
    return true;
}

void rtos::nvs::deinit()
{
    LockGuard lock(storageMutex);
    initialized = false;
}

bool rtos::nvs::erase()
{
    LockGuard lock(storageMutex);
    if (!initialized || nvs_clear(&filesystem) != 0)
        return false;

    resetCollisionTable();
    initialized = false;
    return true;
}

bool Nvs::open(const char* nspace, Mode mode)
{
    close();
    if (!validName(nspace))
        return false;

    LockGuard lock(storageMutex);
    if (!initialized)
        return false;

    auto* handle = new (std::nothrow) Handle{};
    if (handle == nullptr)
        return false;
    std::memcpy(handle->nspace, nspace, std::strlen(nspace) + 1);
    handle->mode = mode;

    handle_ = reinterpret_cast<uintptr_t>(handle);
    opened_ = true;
    return true;
}

void Nvs::close()
{
    if (!opened_)
        return;
    delete toHandle(handle_);
    handle_ = 0;
    opened_ = false;
}

bool Nvs::get_str(const char* key, char* out, size_t& inout_size)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized)
        return false;
    if (!readValue(*toHandle(handle_), key, ValueType::String, out, inout_size))
        return false;
    return out == nullptr ||
        (inout_size != 0 && out[inout_size - 1] == '\0');
}

bool Nvs::get_blob(const char* key, void* out, size_t& inout_size)
{
    LockGuard lock(storageMutex);
    return opened_ && initialized &&
        readValue(*toHandle(handle_), key, ValueType::Blob, out, inout_size);
}

bool Nvs::get_u32(const char* key, uint32_t& out)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized)
        return false;
    size_t size = sizeof(out);
    return readValue(*toHandle(handle_), key, ValueType::U32, &out, size) &&
        size == sizeof(out);
}

bool Nvs::get_i32(const char* key, int32_t& out)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized)
        return false;
    size_t size = sizeof(out);
    return readValue(*toHandle(handle_), key, ValueType::I32, &out, size) &&
        size == sizeof(out);
}

bool Nvs::set_str(const char* key, const char* value)
{
    if (value == nullptr)
        return false;
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized ||
        toHandle(handle_)->mode != Mode::ReadWrite)
        return false;
    return writeValue(
        *toHandle(handle_), key, ValueType::String,
        value, std::strlen(value) + 1);
}

bool Nvs::set_blob(const char* key, const void* data, size_t size)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized ||
        toHandle(handle_)->mode != Mode::ReadWrite)
        return false;
    return writeValue(*toHandle(handle_), key, ValueType::Blob, data, size);
}

bool Nvs::set_u32(const char* key, uint32_t value)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized ||
        toHandle(handle_)->mode != Mode::ReadWrite)
        return false;
    return writeValue(
        *toHandle(handle_), key, ValueType::U32, &value, sizeof(value));
}

bool Nvs::set_i32(const char* key, int32_t value)
{
    LockGuard lock(storageMutex);
    if (!opened_ || !initialized ||
        toHandle(handle_)->mode != Mode::ReadWrite)
        return false;
    return writeValue(
        *toHandle(handle_), key, ValueType::I32, &value, sizeof(value));
}

bool Nvs::erase_key(const char* key)
{
    if (!validName(key))
        return false;

    LockGuard lock(storageMutex);
    if (!opened_ || !initialized ||
        toHandle(handle_)->mode != Mode::ReadWrite)
        return false;

    auto* mapping = findMapping(*toHandle(handle_), key);
    if (mapping == nullptr || nvs_delete(&filesystem, mapping->id) != 0)
        return false;

    const size_t removedIndex = static_cast<size_t>(
        mapping - collisionTable.mappings);
    const size_t lastIndex = collisionTable.count - 1;
    const Mapping removed = *mapping;
    if (removedIndex != lastIndex)
    {
        collisionTable.mappings[removedIndex] =
            collisionTable.mappings[lastIndex];
    }
    collisionTable.mappings[lastIndex] = Mapping{};
    --collisionTable.count;

    if (persistCollisionTable())
        return true;

    ++collisionTable.count;
    if (removedIndex != lastIndex)
    {
        collisionTable.mappings[lastIndex] =
            collisionTable.mappings[removedIndex];
    }
    collisionTable.mappings[removedIndex] = removed;
    return false;
}

bool Nvs::commit()
{
    LockGuard lock(storageMutex);
    return opened_ && initialized;
}
