idf_component_register(
    SRCS
        "backends/espidf/rtos_backend.cpp"
        "backends/espidf/rtos_spi.cpp"
        "backends/espidf/rtos_i2c.cpp"
        "backends/espidf/rtos_nvs.cpp"
        "src/rtos_common.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        driver esp_timer esp_app_format nvs_flash
)
