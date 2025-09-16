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

    void registerMonitor(ParamMonitor* monitor)
    {
        for (size_t i = 0; i < MAX_MONITORS; ++i)
        {
            if (monitorsArray_[i] == nullptr)
            {
                monitorsArray_[i] = monitor;
                return;
            }
        }
    }

private:
    std::array<ParamMonitor*, MAX_MONITORS> monitorsArray_;
};