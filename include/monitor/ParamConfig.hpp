#pragma once
#include <cstddef>
#include <array>
#include <functional>
#include "envelope/envelope.hpp"
#include "topics.hpp"
#include "config/JsonCodec.hpp"
#include "config/Result.hpp"

class ParamConfigJsonCodec : public rtos::config::JsonCodec<ParamConfig>
{
    public:
    Result<void> decode(std::string_view json) const override
    {
        // Implement JSON to ParamConfig parsing logic here
        return ParamConfig{};
    }
    Result<void> encode(const ParamConfig &cfg) const override
    {
        // Implement ParamConfig to JSON serialization logic here
        return std::string{};
    }
};

class ParamConfig
{
public:
    ParamConfig(std::string_view cfgFile = "/cfg/paramConfig.json")
    {
        // Choose a store per target
        auto store = std::make_unique<FsStore>("/spiffs", cfgFile);

        // Choose a codec
        auto codec = std::make_unique<ParamConfigJsonCodec>();
    }
    ~ParamConfig() = default;

private:
};