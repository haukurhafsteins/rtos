idf_component_register(
    SRCS 
        "src/rtos_freertos_espidf.cpp"
    INCLUDE_DIRS
        "." "include"
    REQUIRES
        # other components you need (e.g. freertos, driver, esp_timer)
)
