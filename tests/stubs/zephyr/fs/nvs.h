#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include <zephyr/device.h>

struct nvs_fs
{
    const device* flash_device = nullptr;
    std::size_t offset = 0;
    uint16_t sector_size = 0;
    uint16_t sector_count = 0;
};

int nvs_mount(nvs_fs* filesystem);
ssize_t nvs_read(nvs_fs* filesystem, uint16_t id, void* data, std::size_t length);
ssize_t nvs_write(
    nvs_fs* filesystem, uint16_t id, const void* data, std::size_t length);
int nvs_delete(nvs_fs* filesystem, uint16_t id);
int nvs_clear(nvs_fs* filesystem);
