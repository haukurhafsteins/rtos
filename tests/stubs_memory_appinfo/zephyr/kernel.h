#pragma once

#include <cstddef>

#define CONFIG_MCUBOOT_IMG_MANAGER 1
#define CONFIG_RTOS_PSRAM_HEAP_SIZE 256

struct sys_heap
{
};

struct k_heap
{
    sys_heap heap;
    std::size_t capacity;
};

struct k_timeout_t
{
    int ticks;
};

inline constexpr k_timeout_t K_NO_WAIT{0};

#define K_HEAP_DEFINE(name, bytes) k_heap name{{}, bytes}

void* k_heap_alloc(k_heap* heap, std::size_t bytes, k_timeout_t timeout);
void* k_heap_realloc(
    k_heap* heap, void* pointer, std::size_t bytes, k_timeout_t timeout);
void k_heap_free(k_heap* heap, void* pointer);
