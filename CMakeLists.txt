idf_component_register(
    SRCS
        "backends/espidf/rtos_backend.cpp"
        "backends/espidf/rtos_log_sink.cpp"
        "backends/espidf/rtos_gpio_pinmap.cpp"
        "backends/espidf/rtos_final_log_output.cpp"
        "backends/espidf/rtos_spi.cpp"
        "backends/espidf/rtos_i2c.cpp"
        "backends/espidf/rtos_nvs.cpp"
        "backends/espidf/rtos_watchdog.cpp"
        "backends/espidf/rtos_system.cpp"
        "src/rtos_common.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        driver esp_timer esp_app_format nvs_flash esp_event esp_system
        # ESP-IDF v6.0 finished the driver split: the `driver` umbrella no longer carries
        # driver/gpio.h, driver/spi_master.h or driver/i2c_master.h. Naming the esp_driver_*
        # components explicitly is correct on v5.5.5 too (HMO-113).
        esp_driver_gpio esp_driver_spi esp_driver_i2c
)
