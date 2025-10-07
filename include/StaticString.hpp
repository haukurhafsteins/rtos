#pragma once
#include <string_view>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <RtosLog.hpp>

template <std::size_t N>
class StaticString
{
public:
    StaticString() : len(0)
    {
        clear();
    }

    StaticString(const std::string_view str)
    {
        assign(str);
    }

    void assign(const std::string_view str)
    {
        std::size_t l = std::strlen(str);
        if (l >= N)
        {
            RTOS_LOGE(TAG, "Overflow in StaticString, max size %zu, got %zu", N - 1, l);
            //throw std::overflow_error("StaticString overflow");
        }
        std::memcpy(buffer, str, l + 1);
        len = l;
    }

    void assign(const std::string_view data, std::size_t l)
    {
        if (l >= N)
        {
            RTOS_LOGE(TAG, "Overflow in StaticString, max size %d, got %d", N - 1, l);
            // throw std::overflow_error("StaticString overflow");
        }
        std::memcpy(buffer, data, l);
        buffer[l] = '\0';
        len = l;
    }

    void append(const std::string_view str)
    {
        std::size_t l = std::strlen(str);
        if (len + l >= N)
            throw std::overflow_error("StaticString overflow");
        std::memcpy(buffer + len, str, l + 1);
        len += l;
    }

    // clang-format off
    void         clear() { len = 0; buffer[0] = '\0'; }
    std::size_t  size() const { return len; }
    std::size_t  max_size() const { return N - 1; } // bytes available excluding '\0'
    std::size_t  capacity() const { return N - 1; }
    const char  *c_str() const { return buffer; }
    char         &operator[](std::size_t i) { return buffer[i]; }
    const char   &operator[](std::size_t i) const { return buffer[i]; }
    // clang-format on
private:
    constexpr static const std::string_view TAG = "StaticString";
    char buffer[N]{};
    std::size_t len;
};
