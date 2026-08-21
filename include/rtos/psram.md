# Portable large-allocation heap

The `psram_*` names are retained for source compatibility. Backend semantics
differ by platform:

- ESP-IDF allocates from external PSRAM.
- Linux allocates from the process heap.
- Zephyr allocates without waiting from a dedicated `k_heap` whose static SRAM
  budget is exactly `CONFIG_RTOS_PSRAM_HEAP_SIZE`.

The Zephyr budget must be selected from an application call-site and lifetime
audit. It is not an implicit claim that a workload designed for external PSRAM
fits in on-chip SRAM.

## API

- `psram_malloc(size)` allocates one block.
- `psram_calloc(count, size)` rejects multiplication overflow and zeroes a
  successful allocation.
- `psram_realloc(pointer, size)` resizes a block in the same backend heap.
- `psram_free(pointer)` releases a block; a null pointer is accepted.
- `psram_allocated_size(pointer)` returns zero for null. On Zephyr it returns
  the heap-managed usable size, which can be slightly larger than the requested
  size because of allocator granularity.
