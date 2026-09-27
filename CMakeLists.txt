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
)
