#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    SYS_REBOOT_COLD = 0,
};

void sys_reboot(int mode);

#ifdef __cplusplus
}
#endif
