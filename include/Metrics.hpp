#pragma once
#include <array>

struct Metrics
{
    enum class Unit : int32_t
    {
        none = 0,
        temperature,
        humidity,
        bar,
        l_per_min,
        m3,
        ppm,
        ug_per_m3,
        kg,
    };
    static constexpr std::array<Unit, 9> getArray()
    {
        return {
            Unit::none,
            Unit::temperature,
            Unit::humidity,
            Unit::bar,
            Unit::l_per_min,
            Unit::m3,
            Unit::ppm,
            Unit::ug_per_m3,
            Unit::kg,
        };
    }
};
