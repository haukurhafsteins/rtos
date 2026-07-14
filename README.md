# rtos

Small C++ RTOS abstraction and utility component for embedded projects.

This component is not meant to hide every operating-system or framework detail. Its purpose is to provide a stable project-level API for the RTOS and library features that application code uses most often, while still allowing platform-specific code when that is the right tool.

The current focus is:

- tasks and task sleep/yield helpers
- typed queues
- variable-size message buffers
- message-buffer-backed tasks
- monotonic timing utilities
- lightweight topic/message bus helpers
- logging wrappers and sinks
- GPIO and PSRAM helpers where supported by the backend
- I2C master bus and device
- SPI master bus and device
- application build and device information (`AppInfo`)
- small embedded utilities such as `QMsg`, `Singleton`, fixed strings, ring buffers, envelopes, and online statistics

## Layout and Supported Backends

```text
include/           portable public headers — the complete API, no backend code
src/               portable implementation shared by all backends
backends/espidf/   ESP-IDF / FreeRTOS backend (full support)
backends/zephyr/   Zephyr backend (core primitives; SPI/I2C are compile-only stubs)
backends/linux/    Linux host backend for utilities and tests (SPI/I2C are compile-only stubs)
tests/             host-side unit tests
```

Application code sees only `include/`. A backend is selected by compiling the sources of exactly one `backends/<backend>/` directory together with `src/rtos_common.cpp` — the headers contain no backend conditionals, so no `USE_*` compile definitions are needed.

Each backend directory implements two porting surfaces:

- `rtos_backend.cpp` implements the small procedural interface in `include/backend.hpp` (tasks, queues, message buffers, time, logging, GPIO, PSRAM, `AppInfo`). The portable classes such as `RtosTask` and `RtosQueue<T>` are written once against it.
- `rtos_spi.cpp` / `rtos_i2c.cpp` implement the driver-like classes whose full interface is declared in `include/RtosSpi.hpp` and `include/RtosI2C.hpp`.

When adding new functionality, follow the same rule: simple handle-based primitives go through `backend.hpp` with one implementation per `rtos_backend.cpp`; richer driver-style classes get a complete class declaration in `include/` and a `backends/<backend>/rtos_<name>.cpp` per backend (stubs are fine for backends without support yet).

The top-level `CMakeLists.txt` is currently set up as an ESP-IDF component and builds the `backends/espidf` sources.

## Design Scope

This library intentionally abstracts only common, repeat-use behavior. It does not try to become a complete POSIX, FreeRTOS, Zephyr, or ESP-IDF compatibility layer.

Good fits:

- portable task creation where the application does not care about the native task handle
- message passing between tasks
- task loops with periodic timeout handling
- monotonic time measurement and RTOS sleeps
- simple publish/subscribe of typed values
- shared logging calls across host and embedded builds

Use native platform APIs directly when you need:

- advanced scheduler configuration
- full driver coverage
- uncommon RTOS primitives
- platform-specific power management, interrupts, or memory capabilities
- performance-sensitive code that depends on backend-specific behavior

## Core APIs

### `RtosTask`

`RtosTask` wraps task creation, deletion, sleep, yield, current task lookup, and optional core pinning.

```cpp
#include <RtosTask.hpp>

static void worker(void *arg)
{
    while (true)
    {
        // do work
        RtosTask::sleep_ms(Millis{100});
    }
}

RtosTask task("worker", 4096, 3, worker, nullptr);
task.start();
```

### `RtosQueue<T>`

`RtosQueue<T>` is a typed queue for trivially copyable values. It supports blocking, non-blocking, ISR send/receive variants, count/space introspection, and reset.

```cpp
#include <RtosQueue.hpp>

struct Sample
{
    int value;
};

RtosQueue<Sample> queue(8);
queue.send(Sample{42});

Sample sample{};
if (queue.receive(sample, 100))
{
    // sample.value == 42
}
```

### `RtosMsgBuffer`

`RtosMsgBuffer` carries variable-size messages. It has a raw byte API plus typed helpers for trivially copyable objects.

```cpp
#include <RtosMsgBuffer.hpp>

RtosMsgBuffer messages(256);

uint32_t value = 10;
messages.send_obj(value);

uint32_t received{};
messages.receive_obj(received);
```

### `RtosMsgBufferTask`

`RtosMsgBufferTask<MaxMsgSize>` combines a task and a message buffer. Derive from it when a task should process commands or events from other tasks.

```cpp
#include <RtosMsgBufferTask.hpp>
#include <QMsg.hpp>

enum class WorkerCmd : uint32_t
{
    Ping,
    SetValue,
};

struct SetValue
{
    int value;
};

class WorkerTask : public RtosMsgBufferTask<sizeof(QMsg<WorkerCmd, SetValue>)>
{
public:
    WorkerTask()
        : RtosMsgBufferTask("worker", 4096, 3, 256)
    {
    }

    bool ping()
    {
        QMsg msg(WorkerCmd::Ping);
        return send(&msg, msg.size());
    }

    bool setValue(SetValue value)
    {
        QMsg msg(WorkerCmd::SetValue, value);
        return send(&msg, msg.size());
    }

protected:
    void handleMessage(std::span<const std::byte> data) override
    {
        auto *base = reinterpret_cast<const QMsg<WorkerCmd> *>(data.data());
        switch (base->cmd)
        {
        case WorkerCmd::Ping:
            break;
        case WorkerCmd::SetValue:
        {
            auto *msg = reinterpret_cast<const QMsg<WorkerCmd, SetValue> *>(data.data());
            auto value = msg->getData()->value;
            (void)value;
            break;
        }
        }
    }
};

static WorkerTask worker;

void app_main()
{
    worker.start();
    worker.ping();
}
```

`receiveTimeout()` enables periodic timeout callbacks. Use `RTOS_TASK_WAIT_FOREVER` for the default blocking behavior.

### `rtos::time`

`include/time.hpp` provides `std::chrono`-friendly duration aliases, a monotonic high-resolution clock, and task sleep helpers.

```cpp
#include <time.hpp>

using namespace rtos::time;
using namespace std::chrono_literals;

auto start = HighResClock::now();
sleep_for(20ms);
auto elapsed = std::chrono::duration_cast<Micros>(HighResClock::now() - start);
```

See [`include/TIME.md`](include/TIME.md) for more detail.

### `AppInfo`

`AppInfo` exposes the firmware build description, chip details, and the factory MAC address. The full interface is declared in [`include/AppInfo.hpp`](include/AppInfo.hpp); each backend implements it in its `backends/<backend>/rtos_backend.cpp` file (ESP-IDF reads `esp_app_get_description()`, `esp_chip_info()`, and the eFuse MAC; the Linux host backend returns compile-time fallbacks).

```cpp
#include <AppInfo.hpp>

const auto &desc = rtos::AppInfo::description();
// desc.projectName, desc.version, desc.buildDate, desc.buildTime, desc.sdkVersion

const auto &chip = rtos::AppInfo::chip();
// chip.model, chip.cores, chip.revision, chip.wifi, chip.bluetoothLe, ...

uint8_t mac[rtos::AppInfo::MacSize];
if (rtos::AppInfo::macAddress(mac))
{
    // e.g. derive a serial number string from the MAC
}
```

### `MsgBus` and `Topic<T>`

`MsgBus` provides a small typed publish/subscribe registry. Topics are long-lived, named values; subscribers receive topic updates through an `IRtosMsgReceiver`, commonly a `RtosMsgBufferTask`.

See [`include/MSGBUS.md`](include/MSGBUS.md) for the full API and usage model.

### Other Utilities

Additional utility headers live under `include/`:

- [`include/buffers/RINGBUFFER.md`](include/buffers/RINGBUFFER.md)
- [`include/envelope/ENVELOPE.md`](include/envelope/ENVELOPE.md)
- [`include/Gpio.md`](include/Gpio.md)
- [`include/RTOS_PSRAM_README.md`](include/RTOS_PSRAM_README.md)
- [`RTOS_SPI.md`](RTOS_SPI.md)
- [`I2C.md`](I2C.md)
- [`include/statistics/MinMaxAvg.md`](include/statistics/MinMaxAvg.md)

## ESP-IDF Usage

As an ESP-IDF component, add this repository under your project's `components/rtos` directory or include it as a managed component/submodule. The provided `idf_component.yml` describes the component metadata, and `CMakeLists.txt` registers the ESP-IDF backend sources.

Typical project code should include the abstraction headers rather than native backend headers:

```cpp
#include <RtosTask.hpp>
#include <RtosQueue.hpp>
#include <RtosMsgBufferTask.hpp>
#include <time.hpp>
```

Only backend code should normally include ESP-IDF, FreeRTOS, or Zephyr headers directly.

## Host Tests

Reusable host-side tests live in `tests/`.

```bash
cmake -S tests -B build-rtos-tests
cmake --build build-rtos-tests
ctest --test-dir build-rtos-tests --output-on-failure
```

These tests are written against the component APIs so they can move with the component into other projects.

## Status

This component is evolving with the projects that use it. Prefer small additions that cover common cross-project needs, and keep platform-specific or uncommon functionality in the backend or application layer until it becomes a repeated pattern.
