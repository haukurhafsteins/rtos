#include "rtos/system.hpp"

#include "esp_system.h"

[[noreturn]] void rtos::system::restart()
{
    esp_restart();
}
