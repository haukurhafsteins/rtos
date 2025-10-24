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
        mbar,
        l_per_min,
        m3,
        ppm,
        ug_per_m3,
        g,
        kg,
        tonne,
        iaq,
    };
    static constexpr std::array<Unit, 11> getArray()
    {
        return {
            Unit::none,
            Unit::temperature,
            Unit::humidity,
            Unit::bar,
            Unit::mbar,
            Unit::l_per_min,
            Unit::m3,
            Unit::ppm,
            Unit::ug_per_m3,
            Unit::g,
            Unit::kg,
            Unit::tonne,
            Unit::iaq,
        };
    }
    static constexpr const char *c_str(Unit metric)
    {
        switch (metric) {
            case Unit::temperature: return "temperature";
            case Unit::humidity: return "humidity";
            case Unit::bar: return "bar";
            case Unit::mbar: return "mbar";
            case Unit::l_per_min: return "l_per_min";
            case Unit::m3: return "m3";
            case Unit::ppm: return "ppm";
            case Unit::ug_per_m3: return "ug_per_m3";
            case Unit::g: return "g";
            case Unit::kg: return "kg";
            case Unit::tonne: return "tonne";
            case Unit::iaq: return "iaq_index";
            default: return "none";
        }
    }
};
