# I2C Support

`RtosI2C` provides a thin, portable abstraction over the I2C master API. It is split into two classes that mirror the underlying ESP-IDF model:

| Class | Wraps |
|---|---|
| `RtosI2CBus` | One physical I2C port (shared by all devices on that bus) |
| `RtosI2CDevice` | One device address on a bus |

Both classes are non-copyable and movable. All I/O operations return `bool` (`true` = success).

## Backend support

| Backend | Status |
|---|---|
| ESP-IDF / FreeRTOS | Full — wraps `i2c_master_*` (IDF v5+) |
| Zephyr | Compile-only stub — all methods return `false` |
| Linux | Compile-only stub — all methods return `false` |

## Include

```cpp
#include <RtosI2C.hpp>
```

## RtosI2CBus

### Config

```cpp
struct Config {
    int     port              = 0;     // I2C_NUM_0, I2C_NUM_1, …
    int     sda_pin           = -1;
    int     scl_pin           = -1;
    bool    internal_pullup   = true;
    uint8_t glitch_filter_cnt = 7;
};
```

### Lifecycle

```cpp
RtosI2CBus bus;

RtosI2CBus::Config bus_cfg;
bus_cfg.port    = 0;           // I2C_NUM_0
bus_cfg.sda_pin = 21;
bus_cfg.scl_pin = 22;

if (!bus.init(bus_cfg)) {
    // handle error
}

// later
bus.deinit();                  // also called automatically by destructor
```

`init()` may be called only once. To re-initialise, call `deinit()` first or move a new instance into place.

## RtosI2CDevice

### Config

```cpp
struct Config {
    uint16_t address    = 0;        // 7-bit device address (default) or 10-bit
    uint32_t clk_hz     = 400000;   // SCL frequency in Hz
    bool     addr_10bit = false;    // set true for 10-bit addressing
    int      timeout_ms = 100;      // operation timeout applied to every I/O call
};
```

### Lifecycle

```cpp
RtosI2CDevice dev;

RtosI2CDevice::Config dev_cfg;
dev_cfg.address    = 0x48;
dev_cfg.clk_hz     = 400000;
dev_cfg.timeout_ms = 50;

if (!dev.init(bus, dev_cfg)) {
    // handle error
}

// later
dev.deinit();                  // also called automatically by destructor
```

Multiple devices can be registered on the same bus.

### I/O operations

All operations use the `timeout_ms` stored in the device's `Config`.

#### Write

Send `len` bytes to the device.

```cpp
uint8_t cmd[] = {0x01, 0xA0};
bool ok = dev.write(cmd, sizeof(cmd));
```

#### Read

Receive `len` bytes from the device.

```cpp
uint8_t buf[4];
bool ok = dev.read(buf, sizeof(buf));
```

#### Write-then-read (combined transaction)

Write a register address then read back the result in a single I2C transaction. This is the most common pattern for register-based sensors.

```cpp
uint8_t reg = 0x00;
uint8_t result[2];
bool ok = dev.write_read(&reg, 1, result, sizeof(result));
```

## Full example

```cpp
#include <RtosI2C.hpp>

// Board-level constants
static constexpr int I2C_PORT = 0;
static constexpr int PIN_SDA  = 21;
static constexpr int PIN_SCL  = 22;

// Device address (7-bit)
static constexpr uint16_t SENSOR_ADDR = 0x48;

// Allocate as member variables or statics — not on the stack of a short-lived scope
static RtosI2CBus    g_bus;
static RtosI2CDevice g_sensor;

void app_init()
{
    RtosI2CBus::Config bus_cfg;
    bus_cfg.port    = I2C_PORT;
    bus_cfg.sda_pin = PIN_SDA;
    bus_cfg.scl_pin = PIN_SCL;
    g_bus.init(bus_cfg);

    RtosI2CDevice::Config dev_cfg;
    dev_cfg.address    = SENSOR_ADDR;
    dev_cfg.timeout_ms = 50;
    g_sensor.init(g_bus, dev_cfg);
}

float read_temperature()
{
    uint8_t reg = 0x00;
    uint8_t raw[2] = {};
    if (!g_sensor.write_read(&reg, 1, raw, 2)) {
        return -999.0f;
    }
    int16_t counts = static_cast<int16_t>((raw[0] << 8) | raw[1]) >> 4;
    return counts * 0.0625f;
}
```

## Escape hatch

If you need to pass the raw ESP-IDF handle to existing code that has not yet been migrated, use `native()`:

```cpp
i2c_master_bus_handle_t raw_bus = g_bus.native();
i2c_master_dev_handle_t raw_dev = g_sensor.native();
```

Prefer migrating callers to `RtosI2CDevice` rather than relying on this long-term.
