# GPIO Abstraction Layer (`rtos::gpio`)

This header defines a **platform-independent GPIO interface** for RTOS-based embedded systems.
It provides configuration, interrupt handling, and runtime control of digital I/O pins while allowing backends to implement the platform-specific details.

The design uses a **pImpl (pointer to implementation)** pattern to keep the header portable, with concrete backend implementations provided per-platform (e.g. ESP-IDF, Zephyr, STM32 HAL).

---

## ✨ Features

* Strongly-typed GPIO configuration (mode, pull, drive, active level, etc.)
* Interrupt support with trigger types (rising, falling, both, level)
* Asynchronous event delivery via callbacks or RTOS queues
* Optional software debounce (microsecond resolution)
* Active-level helpers for consistent logic handling
* Backend-agnostic API for cross-platform reuse

---

## 🧬 Namespaces

All symbols are under:

```cpp
namespace rtos::gpio
```

---

## ⚙️ Types Overview

### `Mode`

Defines the direction and electrical mode of the pin:

```cpp
enum class Mode : uint8_t {
    Input,
    Output,
    Alternate, // platform-defined alternate function
    Analog     // no digital buffer / high-Z analog
};
```

### `Pull`

Configures internal pull resistors:

```cpp
enum class Pull : uint8_t { None, Up, Down };
```

### `Drive`

Specifies output drive strength (platform-mapped):

```cpp
enum class Drive : uint8_t { Default, Low, Medium, High };
```

### `Active`

Defines whether the signal is active-high or active-low:

```cpp
enum class Active : uint8_t { High, Low };
```

### `Trigger`

Interrupt trigger configuration:

```cpp
enum class Trigger : uint8_t {
    None,
    Rising,
    Falling,
    Both,
    LevelHigh,
    LevelLow
};
```

### `Electrical`

Open-drain/open-source configuration:

```cpp
struct Electrical {
    bool open_drain = false;
    bool open_source = false;
};
```

### `Config`

Full GPIO configuration descriptor:

```cpp
struct Config {
    Mode mode = Mode::Input;
    Pull pull = Pull::None;
    Drive drive = Drive::Default;
    Electrical electrical = {};
    Active active = Active::High;
    uint8_t alt_function = 0;
};
```

### `Event`

Delivered asynchronously from interrupt handlers:

```cpp
struct Event {
    int pin_id;
    Trigger trigger;
    bool level;
    uint32_t isr_count;
    uint64_t timestamp_us;
};
```

---

## 🤠 Class `Pin`

Represents one GPIO pin with platform-specific backend.

### Factory

```cpp
static Pin make(int pin_id, const Config &cfg = {});
```

Creates and initializes a pin instance mapped by a logical board ID.

---

## 📘 Basic Usage

### Example 1 — Simple Output Pin

```cpp
#include "Gpio.hpp"
using namespace rtos::gpio;

int main() {
    // Create pin 5 as output, active-high
    Pin led = Pin::make(5, { .mode = Mode::Output, .active = Active::High });

    // Toggle LED
    led.write(true);
    led.toggle();
}
```

### Example 2 — Reading an Input Pin

```cpp
Pin button = Pin::make(2, { .mode = Mode::Input, .pull = Pull::Up });

bool pressed = button.read_active();
if (pressed) {
    // Handle button press
}
```

### Example 3 — Interrupt with Callback

```cpp
Pin button = Pin::make(2, { .mode = Mode::Input, .pull = Pull::Up });

button.set_callback([](const Event &ev) {
    if (ev.trigger == Trigger::Falling)
        printf("Button pressed (pin %d, count %lu)\n", ev.pin_id, ev.isr_count);
});

button.enable_interrupt(Trigger::Falling);
```

### Example 4 — Interrupt via RTOS Queue

```cpp
rtos::gpio::RtosQueue<Event> queue;

Pin button = Pin::make(2, { .mode = Mode::Input });
button.attach_queue(&queue);
button.enable_interrupt(Trigger::Both);

// In task context:
Event ev;
if (queue.receive(ev, 100ms)) {
    printf("Pin %d changed to %d\n", ev.pin_id, ev.level);
}
```

### Example 5 — Active-Low LED

```cpp
Pin led = Pin::make(1, { .mode = Mode::Output, .active = Active::Low });

led.write_active(true);  // turns LED ON (since it's active-low)
led.write_active(false); // turns LED OFF
```

---

## ⚡ Debouncing

You can enable software debouncing to filter noisy inputs:

```cpp
button.set_debounce_us(50000); // 50 ms debounce
```

---

## 🔧 Backend Implementation

To add a platform backend:

* Derive from `Pin::Impl`
* Implement methods: `reconfigure`, `read`, `write`, `toggle`, `enable_interrupt`, etc.
* The backend factory (`Pin::make`) should instantiate the appropriate `Impl` for the platform.

---

## 🧱 Thread Safety

Backends are expected to ensure thread-safe operation for:

* Concurrent read/write calls
* ISR-driven callbacks and queue delivery

---

## 📄 License

MIT or project-specific license.

---

## 🥪 Example Integration

If using an RTOS (e.g. FreeRTOS, Zephyr):

* The backend can defer ISR events via task notifications, queues, or deferred callbacks.
* Use the `Event` structure to communicate edges and timestamps to application logic.

---

**Author:** Haukur
**Module:** `rtos::gpio` — portable GPIO abstraction for embedded C++
