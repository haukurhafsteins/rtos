#pragma once
#include <cstddef>

template <typename T>
struct VarRange
{
    T min, max;
};
template <typename T>
inline bool in_range(int v, VarRange<T> r) { return v >= r.min && v <= r.max; }

template <class E>
inline bool in_enum(int v, const E *vals, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if ((int)vals[i] == v)
            return true;
    return false;
}
