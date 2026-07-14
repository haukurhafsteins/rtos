#pragma once
#include <cstddef>

namespace rtos::memory {
    // Allocates memory from the most appropriate heap (PSRAM, DRAM, or host heap)
    void* psram_malloc(std::size_t size);
    void* psram_calloc(std::size_t num, std::size_t size);
    void* psram_realloc(void* ptr, std::size_t size);
    void  psram_free(void* ptr);
    std::size_t psram_allocated_size(void* ptr);
}
