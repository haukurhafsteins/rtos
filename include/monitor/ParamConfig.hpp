#pragma once
#include <cstddef>
#include <array>
#include <functional>
#include "envelope/envelope.hpp"
#include "topics.hpp"
#include "config/JsonCodec.hpp"
#include "config/Result.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

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
        // Open the file, create if missing
        std::ifstream file(cfgFile.data());
        if (!file) {
            // File doesn't exist, create a new one
            std::ofstream newFile(cfgFile.data());
            newFile << "{}";  // Write an empty JSON object
            newFile.close();
        }
        else {
            // File exists, read the contents
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            
            // Choose a codec
            auto codec = std::make_unique<ParamConfigJsonCodec>();
            

        }

        // Choose a store per target
        auto store = std::make_unique<FsStore>("/spiffs", cfgFile);
    }
    ~ParamConfig() = default;

private:
};