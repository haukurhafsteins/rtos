#pragma once

#include <cstddef>

struct sys_heap;

struct sys_memory_stats
{
    std::size_t free_bytes;
    std::size_t allocated_bytes;
    std::size_t max_allocated_bytes;
};

std::size_t sys_heap_usable_size(sys_heap* heap, void* pointer);
int sys_heap_runtime_stats_get(sys_heap* heap, sys_memory_stats* stats);
