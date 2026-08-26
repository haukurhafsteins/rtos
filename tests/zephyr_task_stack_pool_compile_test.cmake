cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED RTOS_ROOT OR NOT DEFINED CXX_COMPILER)
    message(FATAL_ERROR "RTOS_ROOT and CXX_COMPILER are required")
endif()

cmake_path(ABSOLUTE_PATH RTOS_ROOT NORMALIZE)
set(task_source "${RTOS_ROOT}/backends/zephyr/rtos_task_queue.cpp")
set(include_args "-I${RTOS_ROOT}/include" "-I${RTOS_ROOT}/tests/stubs_task")

function(assert_config_default symbol expected)
    file(READ "${RTOS_ROOT}/zephyr/Kconfig" kconfig)
    string(FIND "${kconfig}" "config ${symbol}" block_start)
    if(block_start EQUAL -1)
        message(FATAL_ERROR "missing Kconfig symbol ${symbol}")
    endif()

    string(SUBSTRING "${kconfig}" ${block_start} -1 block)
    string(FIND "${block}" "\nconfig " block_end)
    if(NOT block_end EQUAL -1)
        string(SUBSTRING "${block}" 0 ${block_end} block)
    endif()

    string(FIND "${block}" "default ${expected}" default_position)
    if(default_position EQUAL -1)
        message(FATAL_ERROR
            "${symbol} does not default to ${expected}\nblock:\n${block}")
    endif()
endfunction()

function(compile_configuration result_var)
    execute_process(
        COMMAND
            "${CXX_COMPILER}"
            -std=c++20
            -fsyntax-only
            ${include_args}
            -DCONFIG_NUM_PREEMPT_PRIORITIES=25
            ${ARGN}
            "${task_source}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_stdout
        ERROR_VARIABLE compile_stderr
    )
    set(${result_var} "${compile_result}" PARENT_SCOPE)
    set(${result_var}_OUTPUT "${compile_stdout}${compile_stderr}" PARENT_SCOPE)
endfunction()

function(assert_compiles label)
    compile_configuration(result ${ARGN})
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${label} must compile:\n${result_OUTPUT}")
    endif()
endfunction()

function(assert_rejected label diagnostic)
    compile_configuration(result ${ARGN})
    if(result EQUAL 0)
        message(FATAL_ERROR "${label} unexpectedly compiled")
    endif()
    string(FIND "${result_OUTPUT}" "${diagnostic}" diagnostic_position)
    if(diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "${label} failed without expected diagnostic '${diagnostic}':\n${result_OUTPUT}")
    endif()
endfunction()

assert_config_default(RTOS_TASK_STACK_SMALL_BYTES 0)
assert_config_default(RTOS_TASK_STACK_SMALL_SLOTS 0)
assert_config_default(RTOS_TASK_STACK_LARGE_BYTES 10240)
assert_config_default(RTOS_TASK_STACK_LARGE_SLOTS 16)

assert_compiles(
    "legacy 16 x 10240 pool with the zero-sized class compiled out"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=0
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=10240
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=16
)

assert_compiles(
    "Nordic 3 x 4096 plus 1 x 10240 pool"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=4096
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=3
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=10240
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=1
)

assert_rejected(
    "empty pool"
    "Zephyr task stack pool must have at least one slot"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=0
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=0
)

assert_rejected(
    "enabled zero-byte small class"
    "Enabled small task stacks must not be empty"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=1
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=10240
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=1
)

assert_rejected(
    "enabled zero-byte large class"
    "Enabled large task stacks must not be empty"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=0
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=0
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=1
)

assert_rejected(
    "small class larger than large class"
    "Small task stacks must not exceed large task stacks"
    -DCONFIG_RTOS_TASK_STACK_SMALL_BYTES=12288
    -DCONFIG_RTOS_TASK_STACK_SMALL_SLOTS=1
    -DCONFIG_RTOS_TASK_STACK_LARGE_BYTES=10240
    -DCONFIG_RTOS_TASK_STACK_LARGE_SLOTS=1
)
