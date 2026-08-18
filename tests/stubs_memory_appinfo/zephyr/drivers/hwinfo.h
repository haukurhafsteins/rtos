#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

ssize_t hwinfo_get_device_id(uint8_t* buffer, std::size_t length);
