#pragma once

namespace rtos::system
{

/// Restart the device, or terminate the current process on a host build.
/// This function never returns.
[[noreturn]] void restart();

} // namespace rtos::system
