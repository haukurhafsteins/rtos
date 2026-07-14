// ESP-IDF backend for rtos::watchdog (Task Watchdog Timer, esp_task_wdt).

#include "rtos/watchdog.hpp"

#include "esp_task_wdt.h"

bool rtos::watchdog::init(const Config& cfg)
{
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = static_cast<uint32_t>(cfg.timeout.count()),
        .idle_core_mask = cfg.idle_core_mask,
        .trigger_panic = cfg.panic_on_timeout,
    };

    // The TWDT is usually already running (CONFIG_ESP_TASK_WDT_INIT); in
    // that case init reports ESP_ERR_INVALID_STATE and we reconfigure.
    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_ERR_INVALID_STATE)
        err = esp_task_wdt_reconfigure(&wdt_config);
    return err == ESP_OK;
}

bool rtos::watchdog::deinit()
{
    return esp_task_wdt_deinit() == ESP_OK;
}

bool rtos::watchdog::add()
{
    return esp_task_wdt_add(nullptr) == ESP_OK;
}

bool rtos::watchdog::remove()
{
    return esp_task_wdt_delete(nullptr) == ESP_OK;
}

bool rtos::watchdog::feed()
{
    return esp_task_wdt_reset() == ESP_OK;
}
