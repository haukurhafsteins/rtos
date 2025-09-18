# JSON Configuration Manager
## Mental model
`Parse → Validate → Apply (atomically) → Notify`

**Parse** JSON into a temporary T (never mutate live config while parsing).
**Validate** T using a schema (bounds, required fields, enums).
If valid, **Apply** by swapping atomically under a small lock.
**Notify** observers (subsystems) on success; if any observer rejects, rollback (keep old config).
Persist with atomic write (.tmp + fsync + rename).
Support Versioned migrations on load.
Everything is built around small, predictable allocations and bounded buffers.
## File Structure
```
rtos/
  config/
    include/
      rtos/config/ConfigStore.hpp    // backend interface (FS, NVS/KV, RAM)
      rtos/config/JsonCodec.hpp      // encoder/decoder interface
      rtos/config/Schema.hpp         // validators & helpers
      rtos/config/ConfigManager.hpp  // generic manager<T>
      rtos/config/Observer.hpp       // observer interface
      rtos/config/Result.hpp         // Expected-like error carrier
```
## Quick Start
This example how to implement a config for MyClass.
```c++
struct MyClassConfig {
  float var1;
  uint32_t var2;
  std::vector<std::string> var3;
  int version{1};
};
```
```c++
using namespace rtos::config;

// Storage, codec and the config manager
auto store = std::make_unique<FsStore>("/spiffs", "myclass.json");
auto codec = std::make_unique<MyClassJsonCodec>();
ConfigManager<MyClassConfig> mgr("myclass.json", std::move(store), std::move(codec));

// Observer that gets called with the new config for validation
struct MqttSubsystem : ConfigObserver<MqttConfig> {
  bool on_config_apply(const MqttConfig& newCfg) override {
    return true; // false if cannot apply now
  }
} mqtt;

// Add an observer (one or more) to check if everything is OK
mgr.add_observer(&mqtt);

// The one and only load and verfication of the config
auto res = mgr.load();
if (!res) {
  // log res.error.where / res.error.msg
}

```