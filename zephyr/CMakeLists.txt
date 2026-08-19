get_filename_component(RTOS_MODULE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(rtos_build_enabled FALSE)
foreach(config IN ITEMS
    CONFIG_RTOS_TIME
    CONFIG_RTOS_LOG
    CONFIG_RTOS_ASSERT
    CONFIG_RTOS_TASK_QUEUE
    CONFIG_RTOS_MESSAGE_BUFFER
    CONFIG_RTOS_NVS
    CONFIG_RTOS_SPI
    CONFIG_RTOS_I2C
    CONFIG_RTOS_GPIO
    CONFIG_RTOS_WATCHDOG
    CONFIG_RTOS_PSRAM
    CONFIG_RTOS_APP_INFO
)
    if(${config})
        set(rtos_build_enabled TRUE)
    endif()
endforeach()

if(NOT rtos_build_enabled)
    return()
endif()

zephyr_library_named(rtos)
zephyr_include_directories(${RTOS_MODULE_ROOT}/include)

zephyr_library_sources_ifdef(CONFIG_RTOS_TIME
    ../backends/zephyr/rtos_time.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_LOG
    ../src/rtos_log.cpp
    ../backends/zephyr/rtos_log.cpp
    ../backends/zephyr/rtos_final_log_output.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_ASSERT
    ../backends/zephyr/rtos_assert.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_TASK_QUEUE
    ../backends/zephyr/rtos_task_queue.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_MESSAGE_BUFFER
    ../backends/zephyr/rtos_message_buffer.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_NVS
    ../backends/zephyr/rtos_nvs.cpp
)
# Partition Manager names the NVS partition `storage`; Zephyr's default is
# `storage_partition`. (PM enablement is not visible as a CMake var at module
# processing time - no PM condition.) TODO upstream: Kconfig string.
zephyr_library_compile_definitions_ifdef(CONFIG_RTOS_NVS
    RTOS_NVS_PARTITION=storage)

zephyr_library_sources_ifdef(CONFIG_RTOS_SPI
    ../backends/zephyr/rtos_spi.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_I2C
    ../backends/zephyr/rtos_i2c.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_GPIO
    ../src/rtos_common.cpp
    ../backends/zephyr/rtos_backend.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_WATCHDOG
    ../backends/zephyr/rtos_watchdog.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_PSRAM
    ../backends/zephyr/rtos_psram.cpp
)

zephyr_library_sources_ifdef(CONFIG_RTOS_APP_INFO
    ../backends/zephyr/rtos_app_info.cpp
)
