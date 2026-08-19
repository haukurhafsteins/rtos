#pragma once

using printk_hook_fn_t = int (*)(int character);

extern "C" {
void __printk_hook_install(printk_hook_fn_t hook);
printk_hook_fn_t __printk_get_hook();
}
