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
