# components/rtos/CMakeLists.txt
idf_component_register(
    SRCS 
        "src/rtos_freertos.cpp"
        "src/rtos_zephyr.cpp"
    INCLUDE_DIRS
        "." "include"               # public headers: components/rtos/include/...
    REQUIRES
        # other components you need (e.g. freertos, driver, esp_timer)
)
