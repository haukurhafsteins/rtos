cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED RTOS_ROOT)
    message(FATAL_ERROR "RTOS_ROOT is required")
endif()

cmake_path(ABSOLUTE_PATH RTOS_ROOT NORMALIZE)
string(REGEX REPLACE "/$" "" RTOS_ROOT "${RTOS_ROOT}")

function(zephyr_include_directories)
    set_property(GLOBAL APPEND PROPERTY RTOS_TEST_INCLUDE_DIRS ${ARGN})
endfunction()

macro(zephyr_library_named name)
    set_property(GLOBAL PROPERTY RTOS_TEST_LIBRARY_NAME "${name}")
endmacro()

function(zephyr_library_sources source)
    set_property(GLOBAL APPEND PROPERTY RTOS_TEST_SOURCES ${source} ${ARGN})
endfunction()

function(zephyr_library_compile_definitions_ifdef feature_toggle)
    # Definition bookkeeping is not part of this contract; accept and ignore.
endfunction()

function(zephyr_library_sources_ifdef feature_toggle source)
    if(${${feature_toggle}})
        zephyr_library_sources(${source} ${ARGN})
    endif()
endfunction()

function(assert_equal label actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "${label} mismatch\nexpected: ${expected}\nactual:   ${actual}")
    endif()
endfunction()

set(RTOS_FEATURES
    RTOS_TIME
    RTOS_LOG
    RTOS_ASSERT
    RTOS_TASK_QUEUE
    RTOS_MESSAGE_BUFFER
    RTOS_NVS
    RTOS_SPI
    RTOS_I2C
    RTOS_GPIO
    RTOS_WATCHDOG
    RTOS_PSRAM
    RTOS_APP_INFO
)

foreach(feature IN LISTS RTOS_FEATURES)
    set(CONFIG_${feature} ON)
endforeach()

include("${RTOS_ROOT}/zephyr/CMakeLists.txt")

get_property(library_name GLOBAL PROPERTY RTOS_TEST_LIBRARY_NAME)
assert_equal("library name" "${library_name}" "rtos")

get_property(include_dirs GLOBAL PROPERTY RTOS_TEST_INCLUDE_DIRS)
assert_equal("public include directories" "${include_dirs}" "${RTOS_ROOT}/include")

get_property(sources GLOBAL PROPERTY RTOS_TEST_SOURCES)
set(expected_sources
    ../backends/zephyr/rtos_time.cpp
    ../src/rtos_log.cpp
    ../backends/zephyr/rtos_log.cpp
    ../backends/zephyr/rtos_final_log_output.cpp
    ../backends/zephyr/rtos_assert.cpp
    ../backends/zephyr/rtos_task_queue.cpp
    ../backends/zephyr/rtos_message_buffer.cpp
    ../backends/zephyr/rtos_nvs.cpp
    ../backends/zephyr/rtos_spi.cpp
    ../backends/zephyr/rtos_i2c.cpp
    ../src/rtos_common.cpp
    ../backends/zephyr/rtos_backend.cpp
    ../backends/zephyr/rtos_watchdog.cpp
    ../backends/zephyr/rtos_psram.cpp
    ../backends/zephyr/rtos_app_info.cpp
)
assert_equal("selected sources" "${sources}" "${expected_sources}")

set_property(GLOBAL PROPERTY RTOS_TEST_LIBRARY_NAME "")
set_property(GLOBAL PROPERTY RTOS_TEST_SOURCES "")
set_property(GLOBAL PROPERTY RTOS_TEST_INCLUDE_DIRS "")
foreach(feature IN LISTS RTOS_FEATURES)
    set(CONFIG_${feature} OFF)
endforeach()

include("${RTOS_ROOT}/zephyr/CMakeLists.txt")

get_property(library_name GLOBAL PROPERTY RTOS_TEST_LIBRARY_NAME)
get_property(sources GLOBAL PROPERTY RTOS_TEST_SOURCES)
assert_equal("disabled library" "${library_name}" "")
assert_equal("disabled sources" "${sources}" "")

file(READ "${RTOS_ROOT}/zephyr/Kconfig" kconfig)
foreach(feature IN LISTS RTOS_FEATURES)
    string(FIND "${kconfig}" "config ${feature}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "zephyr/Kconfig is missing config ${feature}")
    endif()
endforeach()

string(FIND "${kconfig}" "config RTOS_PSRAM_HEAP_SIZE" heap_size_position)
if(heap_size_position EQUAL -1)
    message(FATAL_ERROR "zephyr/Kconfig is missing config RTOS_PSRAM_HEAP_SIZE")
endif()

foreach(dependency IN ITEMS
    "depends on MULTITHREADING && RTOS_LOG && HEAP_MEM_POOL_SIZE > 0"
    "depends on MULTITHREADING && HEAP_MEM_POOL_SIZE > 0"
    "depends on TASK_WDT && WATCHDOG"
)
    string(FIND "${kconfig}" "${dependency}" dependency_position)
    if(dependency_position EQUAL -1)
        message(FATAL_ERROR "zephyr/Kconfig is missing dependency: ${dependency}")
    endif()
endforeach()
