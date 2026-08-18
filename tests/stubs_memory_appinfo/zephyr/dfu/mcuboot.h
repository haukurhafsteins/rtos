#pragma once

#include <cstddef>
#include <cstdint>

struct mcuboot_img_sem_ver
{
    uint8_t major;
    uint8_t minor;
    uint16_t revision;
    uint32_t build_num;
};

struct mcuboot_img_header_v1
{
    uint32_t image_size;
    mcuboot_img_sem_ver sem_ver;
};

struct mcuboot_img_header
{
    uint32_t mcuboot_version;
    union
    {
        mcuboot_img_header_v1 v1;
    } h;
};

int boot_read_bank_header(
    uint8_t areaId, mcuboot_img_header* header, std::size_t headerSize);
