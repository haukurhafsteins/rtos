// In-memory Linux backend for rtos::Nvs / rtos::nvs.

#include "rtos/Nvs.hpp"

#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using namespace rtos;

namespace
{
using Blob = std::vector<std::byte>;
using Value = std::variant<std::string, Blob, uint32_t, int32_t>;
using Namespace = std::unordered_map<std::string, Value>;

struct Handle
{
    std::string nspace;
    Nvs::Mode mode;
};

std::mutex storageMutex;
std::unordered_map<std::string, Namespace> storage;
bool initialized = false;

Handle* toHandle(uintptr_t handle)
{
    return reinterpret_cast<Handle*>(handle);
}

Handle* writableHandle(uintptr_t handle, bool opened)
{
    if (!opened || !initialized)
        return nullptr;

    auto* current = toHandle(handle);
    return current->mode == Nvs::Mode::ReadWrite ? current : nullptr;
}

template <typename T>
const T* findValue(const Handle& handle, const char* key)
{
    if (key == nullptr)
        return nullptr;

    const auto namespaceIt = storage.find(handle.nspace);
    if (namespaceIt == storage.end())
        return nullptr;

    const auto valueIt = namespaceIt->second.find(key);
    if (valueIt == namespaceIt->second.end())
        return nullptr;

    return std::get_if<T>(&valueIt->second);
}
} // namespace

bool rtos::nvs::init()
{
    std::lock_guard<std::mutex> lock(storageMutex);
    initialized = true;
    return true;
}

void rtos::nvs::deinit()
{
    std::lock_guard<std::mutex> lock(storageMutex);
    initialized = false;
}

bool rtos::nvs::erase()
{
    std::lock_guard<std::mutex> lock(storageMutex);
    if (!initialized)
        return false;

    storage.clear();
    initialized = false;
    return true;
}

bool Nvs::open(const char* nspace, Mode mode)
{
    close();
    if (nspace == nullptr || nspace[0] == '\0')
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    if (!initialized)
        return false;

    auto namespaceIt = storage.find(nspace);
    if (mode == Mode::ReadOnly && namespaceIt == storage.end())
        return false;
    if (mode == Mode::ReadWrite && namespaceIt == storage.end())
        storage.emplace(nspace, Namespace{});

    auto* handle = new (std::nothrow) Handle{nspace, mode};
    if (handle == nullptr)
        return false;

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
    std::lock_guard<std::mutex> lock(storageMutex);
    if (!opened_ || !initialized)
        return false;

    const auto* value = findValue<std::string>(*toHandle(handle_), key);
    if (value == nullptr)
        return false;

    const size_t required = value->size() + 1;
    if (out == nullptr)
    {
        inout_size = required;
        return true;
    }
    if (inout_size < required)
    {
        inout_size = required;
        return false;
    }

    std::memcpy(out, value->c_str(), required);
    inout_size = required;
    return true;
}

bool Nvs::get_blob(const char* key, void* out, size_t& inout_size)
{
    std::lock_guard<std::mutex> lock(storageMutex);
    if (!opened_ || !initialized)
        return false;

    const auto* value = findValue<Blob>(*toHandle(handle_), key);
    if (value == nullptr)
        return false;

    if (out == nullptr)
    {
        inout_size = value->size();
        return true;
    }
    if (inout_size < value->size())
    {
        inout_size = value->size();
        return false;
    }

    if (!value->empty())
        std::memcpy(out, value->data(), value->size());
    inout_size = value->size();
    return true;
}

bool Nvs::get_u32(const char* key, uint32_t& out)
{
    std::lock_guard<std::mutex> lock(storageMutex);
    if (!opened_ || !initialized)
        return false;

    const auto* value = findValue<uint32_t>(*toHandle(handle_), key);
    if (value == nullptr)
        return false;
    out = *value;
    return true;
}

bool Nvs::get_i32(const char* key, int32_t& out)
{
    std::lock_guard<std::mutex> lock(storageMutex);
    if (!opened_ || !initialized)
        return false;

    const auto* value = findValue<int32_t>(*toHandle(handle_), key);
    if (value == nullptr)
        return false;
    out = *value;
    return true;
}

bool Nvs::set_str(const char* key, const char* value)
{
    if (key == nullptr || value == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    auto* handle = writableHandle(handle_, opened_);
    if (handle == nullptr)
        return false;
    storage[handle->nspace][key] = std::string(value);
    return true;
}

bool Nvs::set_blob(const char* key, const void* data, size_t size)
{
    if (key == nullptr || (data == nullptr && size != 0))
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    auto* handle = writableHandle(handle_, opened_);
    if (handle == nullptr)
        return false;

    Blob value(size);
    if (size != 0)
        std::memcpy(value.data(), data, size);
    storage[handle->nspace][key] = std::move(value);
    return true;
}

bool Nvs::set_u32(const char* key, uint32_t value)
{
    if (key == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    auto* handle = writableHandle(handle_, opened_);
    if (handle == nullptr)
        return false;
    storage[handle->nspace][key] = value;
    return true;
}

bool Nvs::set_i32(const char* key, int32_t value)
{
    if (key == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    auto* handle = writableHandle(handle_, opened_);
    if (handle == nullptr)
        return false;
    storage[handle->nspace][key] = value;
    return true;
}

bool Nvs::erase_key(const char* key)
{
    if (key == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(storageMutex);
    auto* handle = writableHandle(handle_, opened_);
    if (handle == nullptr)
        return false;

    const auto namespaceIt = storage.find(handle->nspace);
    return namespaceIt != storage.end() && namespaceIt->second.erase(key) != 0;
}

bool Nvs::commit()
{
    std::lock_guard<std::mutex> lock(storageMutex);
    return opened_ && initialized;
}
