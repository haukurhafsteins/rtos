#include "rtos/assert.hpp"

#include <zephyr/kernel.h>

#include "rtos/Log.hpp"

namespace rtos::backend
{
[[noreturn]] void assert_fail(
    const char *expression,
    const char *file,
    int line,
    const char *function) noexcept
{
    RTOS_LOGE(
        "rtos", "assertion failed: %s at %s:%d (%s)",
        expression ? expression : "", file ? file : "", line, function ? function : "");
    k_panic();
    for (;;)
    {
    }
}
}
