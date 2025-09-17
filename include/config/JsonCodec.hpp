#pragma once
#include <string>
#include "Result.hpp"

namespace rtos::config
{
    template <class T>
    struct JsonCodec
    {
        virtual ~JsonCodec() = default;
        // Parse JSON -> T (no mutation of live state, bounded allocations)
        virtual Result<T> decode(std::string_view json) const = 0;
        // Serialize T -> JSON (stable key order for diffs)
        virtual Result<std::string> encode(const T &cfg) const = 0;
        // Optional: migrate mutable DOM in-place before decode (by version)
        virtual bool migrate_inplace(std::string &json, int &outVersion) const
        {
            (void)json;
            outVersion = 1;
            return true;
        }
    };
} // namespace rtos::config