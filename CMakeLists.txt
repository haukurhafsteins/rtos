idf_component_register(
    SRCS
        "backends/espidf/rtos_backend.cpp"
        "backends/espidf/rtos_spi.cpp"
        "backends/espidf/rtos_i2c.cpp"
        "src/rtos_common.cpp"
    INCLUDE_DIRS
        "include" "include/statistics" "include/envelope"
    REQUIRES
        driver esp_timer esp_app_format
)
