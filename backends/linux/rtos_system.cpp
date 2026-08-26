#include "rtos/system.hpp"

#include <cstdlib>

[[noreturn]] void rtos::system::restart()
{
    std::abort();
}
