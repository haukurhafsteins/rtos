# Heap statistics by region

`rtos::memory::heap_stats(Region)` answers the two questions application code
keeps asking about memory: how much is left in the heap that RTOS objects and
drivers must come from, and how much in the large external heap. It returns a
`HeapStats` with `free_bytes`, `largest_free_block` and `minimum_free_bytes`
(the low-water mark of `free_bytes` since boot). A field a backend cannot
report is 0.

`free_bytes` can look healthy while `largest_free_block` says that no single
allocation of the size a driver needs can succeed any more, and the
instantaneous figures walk straight past short troughs that
`minimum_free_bytes` remembers. Read all three.

## Regions

| Region     | Meaning                                                        |
| ---------- | -------------------------------------------------------------- |
| `Internal` | on-chip RAM; what RTOS objects and driver descriptors must use |
| `External` | off-chip RAM (PSRAM); zeros when the target has none           |
| `Any`      | whatever plain `malloc()` draws from                           |
| `Dma`      | the part of `Internal` that peripheral DMA can address         |

## Backends

- ESP-IDF: one `heap_caps_get_info()` walk per call, with `Internal` =
  `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`, `External` =
  `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, `Any` = `MALLOC_CAP_DEFAULT` and
  `Dma` = `MALLOC_CAP_DMA`. The three fields are exactly what
  `heap_caps_get_free_size()`, `heap_caps_get_largest_free_block()` and
  `heap_caps_get_minimum_free_size()` return for the same capability mask.
- Zephyr (`CONFIG_RTOS_HEAP_STATS`): `Internal`, `Any` and `Dma` all read the
  kernel system heap through `sys_heap_runtime_stats_get()`; `External` reads
  the `psram_*` compatibility heap when `CONFIG_RTOS_PSRAM` is enabled and is
  zeros otherwise. Zephyr's runtime statistics carry no largest-block figure,
  so `largest_free_block` is always 0, and `minimum_free_bytes` is derived
  from the peak allocation the heap has seen.
- Linux host: `Internal`, `Any` and `Dma` report the process heap's free
  bytes from `mallinfo2()` on glibc 2.33 or newer and zeros elsewhere;
  `External` is zeros. Host code should treat the values as shape, not
  substance.
