#pragma once

#include <cstddef>

#include <zephyr/device.h>

struct flash_pages_info
{
    std::size_t size = 0;
};

inline int flash_get_page_info_by_offs(
    const device*, std::size_t, flash_pages_info* info)
{
    info->size = 4096;
    return 0;
}
