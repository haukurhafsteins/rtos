# components/rtos/CMakeLists.txt
idf_component_register(
    SRCS
        "src/rtos.c"            # add your source files here (c/cc/cpp)
    INCLUDE_DIRS
        "include"               # public headers: components/rtos/include/...
    REQUIRES
        # other components you need (e.g. freertos, driver, esp_timer)
)
