# SPI Support

`RtosSpi` provides a thin, portable abstraction over the SPI master API. It is split into two classes that mirror the underlying ESP-IDF model:

| Class | Wraps |
|---|---|
| `RtosSpiBus` | One physical SPI host (shared by all devices on that bus) |
| `RtosSpiDevice` | One chip-select device on a bus |

Both classes are non-copyable and movable. All I/O operations return `bool` (`true` = success).

## Backend support

| Backend | Status |
|---|---|
| ESP-IDF / FreeRTOS | Full — wraps `spi_bus_initialize` / `spi_bus_add_device` / `spi_device_polling_transmit` |
| Zephyr | Compile-only stub — all methods return `false` |
| Linux | Compile-only stub — all methods return `false` |

## Include

```cpp
#include <RtosSpi.hpp>
```

## RtosSpiBus

### Config

```cpp
struct Config {
    int  host               = SPI2_HOST; // SPI1_HOST=0, SPI2_HOST=1, SPI3_HOST=2
    int  mosi_pin           = -1;
    int  miso_pin           = -1;
    int  sclk_pin           = -1;
    int  max_transfer_bytes = 32;
    Dma  dma                = Dma::Auto; // Dma::Disabled or Dma::Auto
};
```

### Lifecycle

```cpp
RtosSpiBus bus;

RtosSpiBus::Config bus_cfg;
bus_cfg.host               = SPI2_HOST;
bus_cfg.mosi_pin           = PIN_MOSI;
bus_cfg.miso_pin           = PIN_MISO;
bus_cfg.sclk_pin           = PIN_SCLK;
bus_cfg.max_transfer_bytes = 32;

if (!bus.init(bus_cfg)) {
    // handle error
}

// later
bus.deinit();  // also called automatically by destructor
```

`init()` may be called only once per host. To re-initialise, call `deinit()` first or move a new instance into place.

Multiple devices can be added to the same bus.

## RtosSpiDevice

### Config

```cpp
struct Config {
    int      cs_pin       = -1;
    uint32_t clk_hz       = 1000000;  // SPI clock frequency in Hz
    uint8_t  mode         = 0;        // SPI mode 0–3 (encodes CPOL/CPHA)
    uint8_t  command_bits = 0;        // hardware command-phase width in bits (0 = none)
    uint8_t  address_bits = 0;        // hardware address-phase width in bits (0 = none)
    int      queue_size   = 1;        // 1 = polling (no transaction pipelining)
};
```

### Lifecycle

```cpp
RtosSpiDevice dev;

RtosSpiDevice::Config dev_cfg;
dev_cfg.cs_pin       = PIN_CS;
dev_cfg.clk_hz       = 5000000;  // 5 MHz
dev_cfg.mode         = 0;
dev_cfg.command_bits = 8;        // register address in hardware command field

if (!dev.init(bus, dev_cfg)) {
    // handle error
}

// later
dev.deinit();  // also called automatically by destructor
```

### I/O operations

All operations use polling (`spi_device_polling_transmit`).

#### Write

Send `len` bytes to the device. No command phase.

```cpp
uint8_t data[] = {0x3A, 0x01};
bool ok = dev.write(data, sizeof(data));
```

#### Read

Receive `len` bytes from the device. No command phase.

```cpp
uint8_t buf[4];
bool ok = dev.read(buf, sizeof(buf));
```

#### Full-duplex transfer

Send and receive simultaneously. Pass `nullptr` for either direction when unused.

```cpp
uint8_t tx[] = {0xAB, 0xCD};
uint8_t rx[2];
bool ok = dev.transfer(tx, rx, sizeof(tx));
```

#### Command + data transfer

Place `cmd` in the hardware command phase, then send/receive `len` data bytes.
This is the natural fit for register-based SPI devices where the command field
carries the register address. Pass `nullptr` for `tx` or `rx` when only one
direction is needed.

```cpp
// Read: set MSB to indicate read, no tx payload
uint8_t rx_buf[6];
bool ok = dev.transfer_cmd(reg | 0x80, nullptr, rx_buf, sizeof(rx_buf));

// Write: clear MSB, provide tx payload
uint8_t payload[] = {value};
bool ok = dev.transfer_cmd(reg, payload, nullptr, sizeof(payload));
```

## Full example — register-based sensor (IIM42352 style)

```cpp
#include <RtosSpi.hpp>

// Board-level pin constants
static constexpr int PIN_MOSI = 11;
static constexpr int PIN_MISO = 13;
static constexpr int PIN_SCLK = 12;
static constexpr int PIN_CS   = 10;

static RtosSpiBus    g_bus;
static RtosSpiDevice g_sensor;

void sensor_init()
{
    RtosSpiBus::Config bus_cfg;
    bus_cfg.host               = SPI2_HOST;
    bus_cfg.mosi_pin           = PIN_MOSI;
    bus_cfg.miso_pin           = PIN_MISO;
    bus_cfg.sclk_pin           = PIN_SCLK;
    bus_cfg.max_transfer_bytes = 32;
    g_bus.init(bus_cfg);

    RtosSpiDevice::Config dev_cfg;
    dev_cfg.cs_pin       = PIN_CS;
    dev_cfg.clk_hz       = 5000000;  // 5 MHz
    dev_cfg.mode         = 0;
    dev_cfg.command_bits = 8;        // 8-bit register address in command phase
    g_sensor.init(g_bus, dev_cfg);
}

bool sensor_read_reg(uint8_t reg, uint8_t* out, size_t len)
{
    // Read: set bit 7 of the register address to signal a read
    return g_sensor.transfer_cmd(reg | 0x80, nullptr, out, len);
}

bool sensor_write_reg(uint8_t reg, uint8_t value)
{
    return g_sensor.transfer_cmd(reg, &value, nullptr, 1);
}
```

## Escape hatch

If you need to pass raw ESP-IDF handles to code that has not yet been migrated:

```cpp
spi_host_device_t raw_host = g_bus.native();
spi_device_handle_t raw_dev = g_sensor.native();
```

Prefer migrating callers to `RtosSpiDevice` rather than relying on this long-term.
