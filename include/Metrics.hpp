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
    static constexpr const char *c_str(Unit metric)
    {
        switch (metric) {
            case Unit::temperature: return "temperature";
            case Unit::humidity: return "humidity";
            case Unit::bar: return "bar";
            case Unit::l_per_min: return "l_per_min";
            case Unit::m3: return "m3";
            case Unit::ppm: return "ppm";
            case Unit::ug_per_m3: return "ug_per_m3";
            case Unit::kg: return "kg";
            default: return "none";
        }
    }
};
