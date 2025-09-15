#pragma once
#include <cstddef>
#include <functional>
#include "ParamMonitor.hpp"
#include "FixedUnorderedMap.hpp"

class ParamMonitorPool
{
public:
    static constexpr size_t MAX_MONITORS = 16;
    ParamMonitorPool() = default;
    ~ParamMonitorPool() = default;

    size_t create(const char* name)
    {
        if (monitors_.size() < MAX_MONITORS) {
            monitors_.insert_or_assign(name, ParamMonitor{});
            return monitors_.size() - 1;
        }
        return -1;  // Pool is full
    }
private:
    FixedUnorderedMap<const char*, ParamMonitor, MAX_MONITORS> monitors_;
};