#pragma once
#include <cstddef>
#include <array>
#include <functional>
#include "envelope/envelope.hpp"
#include "topics.hpp"

class ParamConfig
{
public:
    ParamConfig(std::string_view cfgFile = "/cfg/paramConfig.json") {
        
    }
    ~ParamConfig() = default;

private:
};