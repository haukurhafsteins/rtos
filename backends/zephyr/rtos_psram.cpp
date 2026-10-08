#include "rtos/psram.hpp"
#include "rtos/ZephyrDiagnostics.hpp"

#include <cstring>
#include <limits>

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>

static_assert(
    CONFIG_RTOS_PSRAM_HEAP_SIZE >= 64,
    "CONFIG_RTOS_PSRAM_HEAP_SIZE must reserve at least 64 bytes");

K_HEAP_DEFINE(rtos_psram_heap, CONFIG_RTOS_PSRAM_HEAP_SIZE);

namespace rtos::memory
{
void* psram_malloc(std::size_t size)
{
    void *result = k_heap_alloc(&rtos_psram_heap, size, K_NO_WAIT);
    if (!result && size != 0)
        zephyr_diagnostics::record_failure(zephyr_diagnostics::Failure::Pipeline);
    return result;
}

void* psram_calloc(std::size_t num, std::size_t size)
{
    if (size != 0 && num > std::numeric_limits<std::size_t>::max() / size) {
        zephyr_diagnostics::record_failure(zephyr_diagnostics::Failure::Pipeline);
        return nullptr;
    }
    const std::size_t bytes = num * size;
    void* pointer = psram_malloc(bytes);
    if (pointer != nullptr)
        std::memset(pointer, 0, bytes);
    return pointer;
}

void* psram_realloc(void* pointer, std::size_t size)
{
    void *result = k_heap_realloc(&rtos_psram_heap, pointer, size, K_NO_WAIT);
    if (!result && size != 0)
        zephyr_diagnostics::record_failure(zephyr_diagnostics::Failure::Pipeline);
    return result;
}

void psram_free(void* pointer)
{
    k_heap_free(&rtos_psram_heap, pointer);
}

std::size_t psram_allocated_size(void* pointer)
{
    if (pointer == nullptr)
        return 0;
    return sys_heap_usable_size(&rtos_psram_heap.heap, pointer);
}
}
