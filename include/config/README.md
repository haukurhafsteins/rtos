# JSON Configuration Manager

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
