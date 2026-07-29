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

add_library(rtos_host_support STATIC
    ../src/rtos_common.cpp
    ../backends/linux/rtos_backend.cpp
    ../backends/espidf/rtos_final_log_output.cpp
    ../backends/linux/rtos_spi.cpp
    ../backends/linux/rtos_i2c.cpp
    ../backends/linux/rtos_nvs.cpp
    ../backends/linux/rtos_watchdog.cpp
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
    ring_buffer_test.cpp
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
