# MinMaxAvg — Embedded-Friendly Min/Max/Average

A tiny, header-only C++ utility for tracking **minimum**, **maximum**, and **average** over a stream of samples with **O(1)** updates, **no dynamic allocation**, and minimal dependencies. Designed for MCUs and other constrained systems.

---

## Features

- **O(1)** per `add()`; constant memory.
- Works with **integers** and **floats**.
- **Configurable accumulator** (`SumT`) to prevent overflow with integer inputs.
- Optional **NaN filtering** at compile time.
- **Fixed-point average** helper for Q formats / milli-units.
- No `<limits>`, no exceptions, no STL containers.

---

## Header & Template

```cpp
// MinMaxAvg.hpp
template <
    typename T,                  // input/sample type (e.g., int16_t, int32_t, float)
    typename SumT = double,      // accumulator type for sum (e.g., int64_t for ints, double for floats)
    bool IGNORE_NAN = false      // if true and T is floating, NaNs are skipped
>
class MinMaxAvg;
```

### Includes required by the header
```cpp
#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <math.h>    // for isnan when IGNORE_NAN = true
```

---

## API Reference

All methods are `O(1)` unless noted.

| Method | Signature | Notes |
|---|---|---|
| Constructor | `MinMaxAvg()` | Calls `reset()` |
| Reset | `void reset()` | Clears count/sum; marks stats as empty |
| Add sample | `void add(T v)` | Updates min/max/sum/count; skips NaN if `IGNORE_NAN` and `T` is floating |
| Add many | `void addMany(const T* data, size_t n)` | Loops `add()` over a buffer |
| Has data? | `bool hasData() const` | `true` after first valid sample |
| Count | `uint32_t getCount() const` | Number of accepted samples |
| Min | `T getMin() const` | Valid only if `hasData()` |
| Max | `T getMax() const` | Valid only if `hasData()` |
| Sum | `SumT getSum() const` | Exposes the accumulator |
| Average | `SumT getAvg() const` | Returns `sum/count` or `0` if empty |
| Avg (rounded int) | `SumT getAvgRounded() const` *(enabled only if `SumT` is integral)* | **Round-to-nearest, half-away-from-zero**; returns `0` if empty |
| Fixed-point avg | `template<typename OutT> OutT getAvgFixed(OutT scale) const` | Returns `(avg * scale)` rounded to nearest as `OutT` (e.g., Q16.16 or milli-units) |

> **Contract:** If `hasData()` is `false`, `getMin()/getMax()` are not meaningful; `getAvg()` returns `0`.

---

## Usage Examples

### 1) Integer samples with wide accumulator
```cpp
using Stats = MinMaxAvg<int32_t, int64_t>;  // 64-bit sum prevents overflow
Stats s;
s.add(10);
s.add(20);
s.add(5);
// s.hasData() == true
int32_t mn = s.getMin();         // 5
int32_t mx = s.getMax();         // 20
int64_t avg = s.getAvgRounded(); // 12 (35/3 rounded to nearest)
```

### 2) Float samples, ignoring NaNs
```cpp
using FStats = MinMaxAvg<float, double, true>; // IGNORE_NAN = true
FStats f;
f.add(NAN);    // ignored
f.add(1.0f);
f.add(2.0f);
double avg = f.getAvg(); // 1.5
```

### 3) Fixed-point outputs
```cpp
MinMaxAvg<int16_t, int32_t> fx;
fx.addMany(samples, count);

// Q16.16 average (scale = 1<<16)
int32_t avg_q16_16 = fx.getAvgFixed<int32_t>(1 << 16);

// Milli-units (scale = 1000); convert back to double if needed
int32_t avg_milli = fx.getAvgFixed<int32_t>(1000);   // e.g., 1234 == 1.234
```

---

## Configuration & Portability

- **Choose `SumT` wisely.** For integer inputs, pick a wider accumulator:
  - `int16_t` → `int32_t` or `int64_t`
  - `int32_t` → `int64_t`
  - For floats, `double` is a good default; on some MCUs `double==float` in size.
- **NaN handling:** `IGNORE_NAN=true` filters NaNs only when `T` is floating. For integer `T`, this flag has no effect.
- **Dependencies:** Only C headers; works in freestanding/embedded environments.
- **Thread/ISR safety:** The class is **not** inherently thread-safe. If updated from ISRs and read from tasks, guard with a critical section or snapshot pattern.

---

## Performance Notes

- **Per `add()`**: 1–2 comparisons, 1 addition, 1 increment, a couple of branches. No heap, no division.
- **Average calculation**: `getAvg()` performs one division. `getAvgFixed()` uses `long double` for precision; if that’s heavy on your MCU, you can:
  - Change `long double` to `double` in the implementation, or
  - Provide an integer-only variant tailored to your range and `scale`.

---

## Numerical Behavior

- **Overflow**: If `SumT` is too small, `sum_` overflows (wraps for integral `SumT`). Prevent by selecting an adequately wide `SumT`.
- **Rounding**:
  - `getAvgRounded()` rounds to nearest with **half-away-from-zero** bias.
  - `getAvgFixed(scale)` rounds to nearest using `±0.5` offset before cast.
- **Empty state**: When no samples were accepted, `getAvg*()` returns `0`; `getMin()/getMax()` must not be used (check `hasData()`).

---

## Memory Footprint

Approximate size (bytes) ≈ `sizeof(uint32_t)` + `sizeof(SumT)` + `2*sizeof(T)` + `sizeof(bool)` + padding.

Examples on a typical 32-bit MCU:
- `T=int32_t`, `SumT=int64_t` → ~24 bytes (due to padding).
- `T=float`, `SumT=double` → ~28–32 bytes (platform-dependent).

---

## Typical Wiring (CMake / Arduino / ESP-IDF)

- **CMake**: add the header to your target include paths.
- **Arduino**: place `MinMaxAvg.hpp` in your `src/` or library folder, then `#include "MinMaxAvg.hpp"`.
- **ESP-IDF**: add to component `include/` and list the include directory in `CMakeLists.txt`.

---

## Example: Safe Snapshot Read

When a producer is updating stats in an ISR and a consumer task wants a consistent view:

```cpp
// In task context with interrupts briefly disabled (or a mutex if not ISR):
uint32_t count; int64_t sum; int32_t mn, mx; bool has;
taskENTER_CRITICAL();
// Copy fields atomically relative to 'add' updates
count = s.getCount();
sum   = s.getSum();
mn    = s.getMin();
mx    = s.getMax();
has   = s.hasData();
taskEXIT_CRITICAL();
```

> Alternatively, keep stats per-producer and merge at a higher level.

---

## Testing Checklist

- Empty state: `hasData()==false`, avg=0.
- Single value: min=max=value; avg=value.
- Mixed signs: rounding behavior correct (check ±0.5 cases).
- NaN inputs (when `IGNORE_NAN=true`): ignored, counts unaffected.
- Large `n`: ensure `SumT` prevents overflow.
- Buffer ingestion via `addMany`.

---

## License & Attribution

You’re free to drop this into your project. If you keep this file as a library, consider adding a short header comment with author/contact and your project’s license.

---

## Quick Copy-Paste: Type Aliases

```cpp
using StatsI16 = MinMaxAvg<int16_t, int32_t>;
using StatsI32 = MinMaxAvg<int32_t, int64_t>;
using StatsF32 = MinMaxAvg<float, double, true>;   // ignore NaNs
```

