#pragma once

#include <cstddef>
#include <cstdint>

namespace zephyr_nvs_test
{
void reset();
std::size_t writeCount(uint16_t id);
void failNextWrite(uint16_t id);
}
