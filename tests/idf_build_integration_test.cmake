cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED RTOS_ROOT)
    message(FATAL_ERROR "RTOS_ROOT is required")
endif()

file(READ "${RTOS_ROOT}/CMakeLists.txt" idf_cmake)
foreach(required_text IN ITEMS
    "backends/espidf/rtos_system.cpp"
    "backends/espidf/rtos_gpio_pinmap.cpp"
    "esp_system"
)
    string(FIND "${idf_cmake}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "ESP-IDF CMake is missing ${required_text}")
    endif()
endforeach()
