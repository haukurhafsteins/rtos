# RingBuffer

Lightweight fixed-size FIFO with optional overwrite behavior and pluggable lock policy. Works with external storage via `initialize()` or embedded storage via `StaticRingBuffer`.

## Basics

- Create and initialize with external buffer:

```cpp
#include "buffers/RingBuffer.hpp"

constexpr std::size_t kCapacity = 16;
int backing[kCapacity];

RingBuffer<int> rb;
rb.initialize(backing, sizeof(backing));
```

- Or use static storage:

```cpp
StaticRingBuffer<int, 16> rb;
```

## Pushing and popping

- `push(const T&)` / `push(T&&)` enqueue if there is space; return `false` when full.
- `push_overwrite(...)` always succeeds and drops the oldest element when full.
- `pop(T& out)` returns `false` if empty; otherwise moves the oldest into `out`.
- `pop_n(T* out, std::size_t max)` pops up to `max` elements into `out`; returns count popped.
- `copy_out(T* out, std::size_t max)` copies up to `max` elements into `out` without removing; returns count copied.

## Accessors

- `peek_span()` returns `{ptr,len}` to a contiguous block of the oldest data (length may be less than `size()` if wrapped).
- Random access from oldest: `operator[](i)` for `0 <= i < size()`.
- Recent access (most recent at `idx=0`): `getRecent(idx)` / `setRecent(idx, value)` throw `std::out_of_range` if `idx >= size()`.
- Absolute wrapping access: `getAt(idx)`, `setAt(idx, value)`, `getPointerAt(idx)` index modulo capacity (asserts buffer is initialized).
- `getLast()` returns the most recently pushed element (asserts non-empty).
- Status helpers: `size()`, `capacity()`, `isEmpty()`, `isFull()`, `elementsBytes()`, `bufferBytes()`, `headIndex()`, `oldestIndex()`, `data()`.

## Concurrency and locks

Pass a lock policy to make operations ISR/thread safe. Provide `lock()` and `unlock()` static methods (no RAII overhead beyond the guard).

```cpp
struct MyLock {
    static inline void lock()   noexcept { /* disable IRQ / take mutex */ }
    static inline void unlock() noexcept { /* enable IRQ / release mutex */ }
};

RingBuffer<uint8_t, MyLock> rb;
rb.initialize(storage, sizeof(storage));
```

All public methods guard with `LockGuard<MyLock>`.

## Resetting

- `reset()` clears indices (contents remain but are considered invalid). Call with a lock policy to avoid races.

## Error handling / contracts

- Must call `initialize()` (unless using `StaticRingBuffer`) before any access; asserts enforce `_data`/`_capacity`.
- Accessors that require valid data (`getRecent`, `setRecent`, `getLast`) check emptiness and bounds and throw when invalid.
