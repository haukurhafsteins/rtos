#pragma once

#include <cstddef>

struct sys_heap;

std::size_t sys_heap_usable_size(sys_heap* heap, void* pointer);
