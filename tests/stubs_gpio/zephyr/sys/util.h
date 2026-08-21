#pragma once

#include <cstddef>

#define BIT(bit) (1UL << (bit))
#define CONTAINER_OF(pointer, type, member) \
    reinterpret_cast<type*>(reinterpret_cast<char*>(pointer) - offsetof(type, member))
