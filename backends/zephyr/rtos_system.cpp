#include "rtos/system.hpp"

#include <zephyr/sys/reboot.h>

[[noreturn]] void rtos::system::restart()
{
    sys_reboot(SYS_REBOOT_COLD);
    __builtin_unreachable();
}
