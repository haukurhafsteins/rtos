#pragma once

#include <cstddef>
#include <cstdint>
#ifdef _MSC_VER
#include <BaseTsd.h>
using ssize_t = SSIZE_T; // MSVC has no ssize_t; the Zephyr API these stubs mirror uses it
#else
#include <sys/types.h>
#endif

ssize_t hwinfo_get_device_id(uint8_t* buffer, std::size_t length);
