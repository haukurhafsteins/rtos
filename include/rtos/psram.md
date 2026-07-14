# PSRAM Memory Management Library

This library provides a set of functions for managing memory allocation in PSRAM (Pseudo Static Random Access Memory). It offers dynamic memory management functions similar to standard C heap management functions but optimized for PSRAM.

## Features

- Allocate and free memory in PSRAM.
- Support for `malloc`, `calloc`, `realloc`, and `free` operations.
- Query available PSRAM size and perform size rounding operations.
- Initialize and shut down PSRAM.

## API Reference

### Initialization and Shutdown

#### `int rtos::memory::psram_init(void *notUsed);`

Initializes the PSRAM memory allocator.

- **Parameters:** `notUsed` (unused, can be `NULL`).
- **Returns:** `0` on success, non-zero on failure.

#### `void rtos::memory::psram_shutdown(void *notUsed);`

Shuts down the PSRAM memory allocator.

- **Parameters:** `notUsed` (unused, can be `NULL`).

### Memory Allocation

#### `void *rtos::memory::psram_malloc(int size);`

Allocates a block of memory of the given `size` in bytes from PSRAM.

- **Parameters:** `size` - Number of bytes to allocate.
- **Returns:** Pointer to allocated memory or `NULL` on failure.

#### `void *rtos::memory::psram_calloc(int num, int size);`

Allocates memory for an array of `num` elements, each of `size` bytes, initializing all bytes to zero.

- **Parameters:**
  - `num` - Number of elements.
  - `size` - Size of each element in bytes.
- **Returns:** Pointer to allocated memory or `NULL` on failure.

#### `void *rtos::memory::psram_realloc(void *ptr, int size);`

Reallocates a previously allocated block of memory to a new size.

- **Parameters:**
  - `ptr` - Pointer to previously allocated memory.
  - `size` - New size in bytes.
- **Returns:** Pointer to reallocated memory or `NULL` on failure.

#### `void rtos::memory::psram_free(void *ptr);`

Frees a previously allocated block of memory in PSRAM.

- **Parameters:** `ptr` - Pointer to memory to free.

### Utility Functions

#### `int rtos::memory::psram_size(void *ptr);`

Returns the size of a given allocated memory block in PSRAM.

- **Parameters:** `ptr` - Pointer to allocated memory.
- **Returns:** Size of the allocated memory block in bytes.

#### `int rtos::memory::psram_roundup(int size);`

Rounds up a given size to the nearest valid PSRAM allocation size.

- **Parameters:** `size` - Size to round up.
- **Returns:** Rounded-up size in bytes.

#### `int rtos::memory::psram_get_free_size();`

Returns the amount of free memory available in PSRAM.

- **Returns:** Number of free bytes available in PSRAM.

## Usage Example

```c
#include "rtos/psram.hpp"

int main() {
    rtos::memory::psram_init(NULL);
    
    void *buffer = rtos::memory::psram_malloc(1024);
    if (buffer) {
        // Use buffer
        rtos::memory::psram_free(buffer);
    }
    
    rtos::memory::psram_shutdown(NULL);
    return 0;
}
```

## License

This library is licensed under the MIT License.
