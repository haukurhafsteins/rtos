#include <type_traits>

#include "rtos/system.hpp"

static_assert(std::is_same_v<decltype(&rtos::system::restart), void (*)()>);

int main()
{
    const auto restart = &rtos::system::restart;
    return restart == nullptr;
}
