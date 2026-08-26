cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED RTOS_ROOT)
    message(FATAL_ERROR "RTOS_ROOT is required")
endif()

file(READ "${RTOS_ROOT}/include/rtos/MsgBufferTask.hpp" header)
string(FIND "${header}" "#include <span>" span_include_position)
if(span_include_position EQUAL -1)
    message(FATAL_ERROR
        "MsgBufferTask.hpp must directly include <span> for standalone C++20 use")
endif()
