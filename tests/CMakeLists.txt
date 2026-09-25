cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(rtos_host_tests LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(CTest)
if(NOT BUILD_TESTING)
    return()
endif()

option(RTOS_FETCH_GTEST "Download GoogleTest when not installed" ON)

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    if(NOT RTOS_FETCH_GTEST)
        message(FATAL_ERROR "GTest not found and RTOS_FETCH_GTEST is OFF")
    endif()

    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

add_test(
    NAME zephyr_build_integration_contract
    COMMAND
        ${CMAKE_COMMAND}
        -DRTOS_ROOT=${CMAKE_CURRENT_LIST_DIR}/..
        -P ${CMAKE_CURRENT_LIST_DIR}/zephyr_build_integration_test.cmake
)

add_test(
    NAME zephyr_task_stack_pool_compile_contract
    COMMAND
        ${CMAKE_COMMAND}
        -DRTOS_ROOT=${CMAKE_CURRENT_LIST_DIR}/..
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -P ${CMAKE_CURRENT_LIST_DIR}/zephyr_task_stack_pool_compile_test.cmake
)

add_test(
    NAME msg_buffer_task_header_self_containment_contract
    COMMAND
        ${CMAKE_COMMAND}
        -DRTOS_ROOT=${CMAKE_CURRENT_LIST_DIR}/..
        -P ${CMAKE_CURRENT_LIST_DIR}/header_self_containment_test.cmake
)

add_test(
    NAME idf_build_integration_contract
    COMMAND
        ${CMAKE_COMMAND}
        -DRTOS_ROOT=${CMAKE_CURRENT_LIST_DIR}/..
        -P ${CMAKE_CURRENT_LIST_DIR}/idf_build_integration_test.cmake
)

add_library(rtos_host_support STATIC
    ../src/rtos_common.cpp
    ../src/rtos_log.cpp
    ../backends/linux/rtos_backend.cpp
    ../backends/espidf/rtos_final_log_output.cpp
    ../backends/linux/rtos_spi.cpp
    ../backends/linux/rtos_i2c.cpp
    ../backends/linux/rtos_nvs.cpp
    ../backends/linux/rtos_watchdog.cpp
    ../backends/linux/rtos_system.cpp
)

target_include_directories(rtos_host_support
    PUBLIC
        ../include
    PRIVATE
        stubs
)

add_executable(rtos_host_tests
    final_log_output_test.cpp
    log_test.cpp
    memory_stats_test.cpp
    nvs_test.cpp
    ring_buffer_test.cpp
    system_test.cpp
)

target_link_libraries(rtos_host_tests
    PRIVATE
        rtos_host_support
        GTest::gtest_main
)

target_include_directories(rtos_host_tests PRIVATE stubs)

if(MSVC)
    target_compile_options(rtos_host_support PRIVATE /W4)
    target_compile_options(rtos_host_tests PRIVATE /W4)
else()
    target_compile_options(rtos_host_support PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(rtos_host_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_host_tests)

add_executable(rtos_zephyr_task_queue_tests
    zephyr_task_queue_test.cpp
    ../backends/zephyr/rtos_task_queue.cpp
)

target_link_libraries(rtos_zephyr_task_queue_tests PRIVATE GTest::gtest_main)
target_include_directories(rtos_zephyr_task_queue_tests PRIVATE ../include stubs_task)
target_compile_definitions(rtos_zephyr_task_queue_tests
    PRIVATE
        CONFIG_RTOS_TASK_STACK_SMALL_BYTES=4096
        CONFIG_RTOS_TASK_STACK_SMALL_SLOTS=3
        CONFIG_RTOS_TASK_STACK_LARGE_BYTES=10240
        CONFIG_RTOS_TASK_STACK_LARGE_SLOTS=1
)

if(MSVC)
    target_compile_options(rtos_zephyr_task_queue_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_task_queue_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_zephyr_task_queue_tests)

foreach(watchdog_fallback IN ITEMS 0 1)
    add_executable(rtos_zephyr_watchdog_fallback_${watchdog_fallback}_tests
        zephyr_watchdog_stubs.cpp
        zephyr_watchdog_test.cpp
        ../backends/zephyr/rtos_watchdog.cpp
    )
    target_link_libraries(rtos_zephyr_watchdog_fallback_${watchdog_fallback}_tests
        PRIVATE GTest::gtest_main)
    target_include_directories(rtos_zephyr_watchdog_fallback_${watchdog_fallback}_tests
        PRIVATE ../include stubs_watchdog)
    target_compile_definitions(rtos_zephyr_watchdog_fallback_${watchdog_fallback}_tests
        PRIVATE CONFIG_TASK_WDT_HW_FALLBACK=${watchdog_fallback})
    gtest_discover_tests(rtos_zephyr_watchdog_fallback_${watchdog_fallback}_tests
        TEST_PREFIX "Fallback${watchdog_fallback}.")
endforeach()

add_executable(rtos_cpp20_header_compatibility_test
    cpp20_header_compatibility_test.cpp
)
target_include_directories(rtos_cpp20_header_compatibility_test PRIVATE ../include)
add_test(
    NAME Cpp20HeaderCompatibilityTest.MutexAndRingBufferCoexist
    COMMAND rtos_cpp20_header_compatibility_test
)

add_executable(rtos_msg_buffer_task_header_compile_test
    msg_buffer_task_header_compile_test.cpp
)
target_include_directories(rtos_msg_buffer_task_header_compile_test PRIVATE ../include)
add_test(
    NAME MsgBufferTaskHeaderCompileTest.StandaloneCxx20
    COMMAND rtos_msg_buffer_task_header_compile_test
)

add_executable(rtos_system_header_compile_test
    system_header_compile_test.cpp
)
target_link_libraries(rtos_system_header_compile_test PRIVATE rtos_host_support)
add_test(
    NAME SystemHeaderCompileTest.PublicHeaderLinks
    COMMAND rtos_system_header_compile_test
)

add_executable(rtos_zephyr_system_tests
    zephyr_system_test.cpp
    ../backends/zephyr/rtos_system.cpp
)
target_link_libraries(rtos_zephyr_system_tests PRIVATE GTest::gtest_main)
target_include_directories(rtos_zephyr_system_tests PRIVATE ../include stubs)
if(MSVC)
    target_compile_options(rtos_zephyr_system_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_system_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(rtos_zephyr_system_tests)

add_executable(rtos_zephyr_nvs_tests
    zephyr_nvs_stubs.cpp
    zephyr_nvs_test.cpp
    ../backends/zephyr/rtos_nvs.cpp
)

target_link_libraries(rtos_zephyr_nvs_tests
    PRIVATE
        GTest::gtest_main
)

target_include_directories(rtos_zephyr_nvs_tests
    PRIVATE
        ../include
        stubs
)

if(MSVC)
    target_compile_options(rtos_zephyr_nvs_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_nvs_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_zephyr_nvs_tests)

add_executable(rtos_zephyr_bus_tests
    zephyr_bus_stubs.cpp
    zephyr_bus_test.cpp
    ../backends/zephyr/rtos_spi.cpp
    ../backends/zephyr/rtos_i2c.cpp
)

target_link_libraries(rtos_zephyr_bus_tests
    PRIVATE
        GTest::gtest_main
)

target_include_directories(rtos_zephyr_bus_tests
    PRIVATE
        ../include
        stubs
)

if(MSVC)
    target_compile_options(rtos_zephyr_bus_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_bus_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_zephyr_bus_tests)

add_executable(rtos_zephyr_gpio_tests
    zephyr_gpio_stubs.cpp
    zephyr_gpio_test.cpp
    ../src/rtos_common.cpp
    ../backends/zephyr/rtos_backend.cpp
)

target_link_libraries(rtos_zephyr_gpio_tests
    PRIVATE
        GTest::gtest_main
)

target_include_directories(rtos_zephyr_gpio_tests
    PRIVATE
        ../include
        stubs_gpio
        stubs
)

if(MSVC)
    target_compile_options(rtos_zephyr_gpio_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_gpio_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_zephyr_gpio_tests)

add_executable(rtos_espidf_gpio_pinmap_tests
    espidf_gpio_pinmap_test.cpp
    ../backends/espidf/rtos_gpio_pinmap.cpp
)

target_link_libraries(rtos_espidf_gpio_pinmap_tests PRIVATE GTest::gtest_main)
target_include_directories(rtos_espidf_gpio_pinmap_tests
    PRIVATE
        ../include
        ../backends/espidf
        stubs_gpio_espidf
)

if(MSVC)
    target_compile_options(rtos_espidf_gpio_pinmap_tests PRIVATE /W4)
else()
    target_compile_options(rtos_espidf_gpio_pinmap_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_espidf_gpio_pinmap_tests)

add_executable(rtos_zephyr_memory_appinfo_tests
    zephyr_memory_appinfo_stubs.cpp
    zephyr_memory_appinfo_test.cpp
    ../backends/zephyr/rtos_psram.cpp
    ../backends/zephyr/rtos_memory.cpp
    ../backends/zephyr/rtos_app_info.cpp
)

target_link_libraries(rtos_zephyr_memory_appinfo_tests
    PRIVATE
        GTest::gtest_main
)

target_include_directories(rtos_zephyr_memory_appinfo_tests
    PRIVATE
        ../include
        stubs_memory_appinfo
)

if(MSVC)
    target_compile_options(rtos_zephyr_memory_appinfo_tests PRIVATE /W4)
else()
    target_compile_options(rtos_zephyr_memory_appinfo_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_zephyr_memory_appinfo_tests)

add_executable(rtos_espidf_log_sink_tests
    espidf_log_sink_test.cpp
    ../backends/espidf/rtos_log_sink.cpp
)

target_link_libraries(rtos_espidf_log_sink_tests PRIVATE GTest::gtest_main)
target_include_directories(rtos_espidf_log_sink_tests
    PRIVATE
        ../include
        stubs_log_espidf
)

if(MSVC)
    target_compile_options(rtos_espidf_log_sink_tests PRIVATE /W4)
else()
    target_compile_options(rtos_espidf_log_sink_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(rtos_espidf_log_sink_tests)
