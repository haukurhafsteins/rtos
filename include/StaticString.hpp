#include <cstring>
#include <cstdio>
#include <stdexcept>

template <std::size_t N>
class StaticString {
public:
    StaticString() : len(0) {
        buffer[0] = '\0';
    }

    StaticString(const char* str) {
        assign(str);
    }

    void assign(const char* str) {
        std::size_t l = std::strlen(str);
        if (l >= N) throw std::overflow_error("StaticString overflow");
        std::memcpy(buffer, str, l + 1);
        len = l;
    }

    void append(const char* str) {
        std::size_t l = std::strlen(str);
        if (len + l >= N) throw std::overflow_error("StaticString overflow");
        std::memcpy(buffer + len, str, l + 1);
        len += l;
    }

    void clear() {
        len = 0;
        buffer[0] = '\0';
    }

    std::size_t size() const {
        return len;
    }

    std::size_t capacity() const {
        return N - 1;
    }

    const char* c_str() const {
        return buffer;
    }

    char& operator[](std::size_t i) {
        return buffer[i];
    }

    const char& operator[](std::size_t i) const {
        return buffer[i];
    }

private:
    char buffer[N];
    std::size_t len;
};
