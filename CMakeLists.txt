idf_component_register(
    SRCS 
        "src/rtos_freertos_espidf.cpp" "src/rtos_common.cpp"
    INCLUDE_DIRS
        "." "include" "include/statistics" "include/envelope"
    REQUIRES
        # other components you need (e.g. freertos, driver, esp_timer)
        driver
)
