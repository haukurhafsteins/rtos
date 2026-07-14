// ESP-IDF backend for rtos::Nvs / rtos::nvs (nvs_flash component).

#include "rtos/Nvs.hpp"

#include "nvs_flash.h"
#include "nvs.h"

using namespace rtos;

namespace
{
nvs_handle_t to_handle(uintptr_t handle)
{
    return static_cast<nvs_handle_t>(handle);
}
} // namespace

// ---------------------------------------------------------------------------
// rtos::nvs — partition lifecycle
// ---------------------------------------------------------------------------

bool rtos::nvs::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Partition was truncated or written by an incompatible layout:
        // erase and retry.
        if (nvs_flash_erase() != ESP_OK)
            return false;
        err = nvs_flash_init();
    }
    return err == ESP_OK;
}

void rtos::nvs::deinit()
{
    nvs_flash_deinit();
}

bool rtos::nvs::erase()
{
    return nvs_flash_erase() == ESP_OK;
}

// ---------------------------------------------------------------------------
// rtos::Nvs — namespace handle
// ---------------------------------------------------------------------------

bool Nvs::open(const char* nspace, Mode mode)
{
    close();

    nvs_handle_t handle = 0;
    if (nvs_open(nspace, mode == Mode::ReadOnly ? NVS_READONLY : NVS_READWRITE, &handle) != ESP_OK)
        return false;

    handle_ = handle;
    opened_ = true;
    return true;
}

void Nvs::close()
{
    if (opened_)
    {
        nvs_close(to_handle(handle_));
        handle_ = 0;
        opened_ = false;
    }
}

bool Nvs::get_str(const char* key, char* out, size_t& inout_size)
{
    return opened_ && nvs_get_str(to_handle(handle_), key, out, &inout_size) == ESP_OK;
}

bool Nvs::get_blob(const char* key, void* out, size_t& inout_size)
{
    return opened_ && nvs_get_blob(to_handle(handle_), key, out, &inout_size) == ESP_OK;
}

bool Nvs::get_u32(const char* key, uint32_t& out)
{
    return opened_ && nvs_get_u32(to_handle(handle_), key, &out) == ESP_OK;
}

bool Nvs::get_i32(const char* key, int32_t& out)
{
    return opened_ && nvs_get_i32(to_handle(handle_), key, &out) == ESP_OK;
}

bool Nvs::set_str(const char* key, const char* value)
{
    return opened_ && nvs_set_str(to_handle(handle_), key, value) == ESP_OK;
}

bool Nvs::set_blob(const char* key, const void* data, size_t size)
{
    return opened_ && nvs_set_blob(to_handle(handle_), key, data, size) == ESP_OK;
}

bool Nvs::set_u32(const char* key, uint32_t value)
{
    return opened_ && nvs_set_u32(to_handle(handle_), key, value) == ESP_OK;
}

bool Nvs::set_i32(const char* key, int32_t value)
{
    return opened_ && nvs_set_i32(to_handle(handle_), key, value) == ESP_OK;
}

bool Nvs::erase_key(const char* key)
{
    return opened_ && nvs_erase_key(to_handle(handle_), key) == ESP_OK;
}

bool Nvs::commit()
{
    return opened_ && nvs_commit(to_handle(handle_)) == ESP_OK;
}
