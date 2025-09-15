# `rtos::time` — timing utilities for RTOS targets

This header provides a small, `std::chrono`-friendly timing layer that works consistently across RTOS backends (FreeRTOS, Zephyr, …). It exposes a monotonic high-resolution clock (microseconds since boot), convenient duration aliases, and simple sleep helpers.

> File: `time.hpp`

---

## Key ideas

* **Monotonic, not wall clock.** `HighResClock` counts up from boot and never goes backward. It isn’t affected by NTP, RTC changes, or timezone.
* **[`std::chrono`](https://en.cppreference.com/w/cpp/chrono.html) integration.** Types are standard `duration`/`time_point` so all of the usual chrono arithmetic and literals work.
* **Backend agnostic.** `now()`/`sleep_*` are implemented per RTOS (e.g., map to `vTaskDelay`/`k_sleep`), so your app code stays portable.

---

## API reference

### Clock

```cpp
struct rtos::time::HighResClock {
  using rep        = int64_t;
  using period     = std::micro; // microsecond tick
  using duration   = std::chrono::microseconds;
  using time_point = std::chrono::time_point<HighResClock>;
  static constexpr bool is_steady = true;

  static time_point now() noexcept; // µs since boot
};
```

* **Resolution:** microseconds.
* **Range:** 64-bit microseconds (\~292,000 years) — overflow is effectively a non-issue for embedded lifetimes.

### Duration aliases

```cpp
using rtos::time::Micros  = std::chrono::microseconds;
using rtos::time::Millis  = std::chrono::milliseconds;
using rtos::time::Seconds = std::chrono::seconds;
```

### Sleep helpers

```cpp
void   rtos::time::sleep_for(Millis ms) noexcept;
void   rtos::time::sleep_until(HighResClock::time_point tp) noexcept;
Millis rtos::time::now_ms() noexcept; // ms since boot
Seconds rtos::time::now_s() noexcept; // s since boot
```

> ⚠️ **Context:** `sleep_for`/`sleep_until` suspend the current *task/thread*. They are **not** safe in ISRs. `HighResClock::now()` is typically safe in ISRs (backend-dependent), but avoid heavy chrono math there.

---

## Usage patterns & examples

All examples assume:

```cpp
#include "time.hpp"
using namespace rtos::time;
using namespace std::chrono_literals; // enables 10ms, 250us, 1s, etc.
```

### 1) Measure elapsed time

```cpp
auto t0 = HighResClock::now();
// ... work ...
auto t1 = HighResClock::now();
auto dt = std::chrono::duration_cast<Micros>(t1 - t0);
printf("Work took %lld µs\n", static_cast<long long>(dt.count()));
```

### 2) Fixed-rate loop (e.g., 100 Hz control task)

```cpp
void control_task() {
  auto next = HighResClock::now();
  const auto period = 10ms; // 100 Hz

  for (;;) {
    // --- do control work here ---

    next += period;                 // advance deadline
    sleep_until(next);              // wait (cooperatively)
    // If we overran, sleep_until() returns immediately
  }
}
```

### 3) Deadline with timeout

```cpp
bool wait_until_ready(HighResClock::time_point deadline) {
  while (!is_ready()) {
    if (HighResClock::now() >= deadline) return false; // timed out
    sleep_for(1ms);
  }
  return true;
}

// usage:
auto ok = wait_until_ready(HighResClock::now() + 250ms);
```

### 4) Simple exponential backoff

```cpp
for (Millis backoff = 10ms; ; backoff = std::min(backoff * 2, 2s)) {
  if (try_connect()) break;
  sleep_for(backoff);
}
```

### 5) Convert between units

```cpp
Micros us = 1500us;
Millis ms = std::chrono::duration_cast<Millis>(us); // -> 1ms (truncates)
Seconds s = 2s;

printf("%lld ms since boot\n", static_cast<long long>(now_ms().count()));
printf("%lld s  since boot\n", static_cast<long long>(now_s().count()));
```

### 6) Sleep until wall-clock aligned cadence (without RTC)

Even without a real wall clock, you can align to a cadence since boot:

```cpp
// Trigger something every whole second boundary since boot.
void once_per_second() {
  for (;;) {
    auto now  = HighResClock::now();
    auto next = HighResClock::time_point{
      std::chrono::duration_cast<Seconds>(now.time_since_epoch()) + 1s
    };
    sleep_until(next);
    tick(); // called at ~1 Hz boundaries
  }
}
```

### 7) Bounded busy-wait (sub-millisecond waits)

`sleep_for` takes milliseconds, which maps well to RTOS tick sleeps. For shorter delays, prefer hardware timers, but a **bounded** spin can be acceptable in rare cases:

```cpp
void delay_us_busy(uint32_t us) {
  auto start = HighResClock::now();
  while (std::chrono::duration_cast<Micros>(HighResClock::now() - start).count() < us) {
    // optionally cpu_relax();
  }
}
```

> ⚠️ Use sparingly; this burns CPU and can impact real-time behavior.

---

## Semantics & portability notes

* **Monotonicity:** `HighResClock::now()` is steady (`is_steady == true`), so time deltas are reliable even if the system RTC changes.
* **Sleep accuracy:** Actual sleep duration depends on backend tick configuration and tickless idle. Expect typical ±1 tick jitter plus wake-up latency.
* **Power modes:** If the MCU sleeps deeply, the backend must ensure `HighResClock` continues counting (e.g., using a low-power timer). Check your platform’s backend notes.
* **ISR usage:** Do **not** call `sleep_for`/`sleep_until` in ISRs. `now()` is usually fine, but verify the backend’s `now()` source is ISR-safe.
* **No wall time:** This module intentionally avoids calendar/UTC. To timestamp for logging, store “since boot” and translate to wall time elsewhere if needed.

---

## Design rationale

* **Microsecond clock:** Many drivers and control loops need sub-millisecond resolution for measurements; sleeps remain millisecond-granular to match RTOS primitives.
* [**`std::chrono` types:**](https://en.cppreference.com/w/cpp/chrono.html) Enables compile-time unit safety and clean arithmetic (`10ms`, `250us`, `1s`), minimizing foot-guns.

---

## Cheatsheet

* **Now:** `auto t = HighResClock::now();`
* **Elapsed:** `auto us = std::chrono::duration_cast<Micros>(HighResClock::now() - t);`
* **Sleep ms:** `sleep_for(5ms);`
* **Periodic:** keep a `next` deadline and `sleep_until(next += period);`
* **Since boot:** `now_ms().count()` / `now_s().count()`

---

## Minimal example

```cpp
#include "time.hpp"
#include <cstdio>
using namespace rtos::time;
using namespace std::chrono_literals;

void app_main() {
  printf("Uptime: %lld ms\n", static_cast<long long>(now_ms().count()));

  auto start = HighResClock::now();
  sleep_for(20ms);
  auto dt = std::chrono::duration_cast<Micros>(HighResClock::now() - start);
  printf("Slept ~%lld µs\n", static_cast<long long>(dt.count()));

  // 50 Hz loop for 100 iterations
  auto next = HighResClock::now();
  for (int i = 0; i < 100; ++i) {
    // do work...
    next += 20ms;
    sleep_until(next);
  }
}
```

---

## Backend implementation sketch (for reference)

> You don’t need this to *use* the API, but it helps when porting.

```cpp
// FreeRTOS example (pseudo-code)
HighResClock::time_point HighResClock::now() noexcept {
  uint64_t us = platform_microsecond_timer(); // monotonic µs since boot
  return time_point{duration{static_cast<int64_t>(us)}};
}

void sleep_for(Millis ms) noexcept {
  vTaskDelay(pdMS_TO_TICKS(ms.count()));
}

void sleep_until(HighResClock::time_point tp) noexcept {
  auto now = HighResClock::now();
  if (tp > now) {
    auto remain = std::chrono::duration_cast<Millis>(tp - now);
    sleep_for(remain);
  }
}
```
