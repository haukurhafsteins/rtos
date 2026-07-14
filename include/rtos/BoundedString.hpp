#pragma once
#include <string_view>
#include <cstring>
#include <stdexcept>

class BoundedString {
public:
    // capacity_including_null is the total buffer size (including room for '\0')
    BoundedString(char* buf, std::size_t capacity_including_null) noexcept
        : buffer_(buf), cap_(capacity_including_null), len_(0) {
        if (cap_ == 0) { buffer_ = nullptr; }
        clear();
    }

    // (Re)bind to a new external buffer
    void bind(char* buf, std::size_t capacity_including_null) noexcept {
        buffer_ = buf;
        cap_    = capacity_including_null;
        len_    = 0;
        if (buffer_ && cap_) buffer_[0] = '\0';
    }

    void assign(std::string_view sv) {
        ensure_bound();
        const std::size_t l = sv.size();
        if (l >= cap_) {
            // throw std::overflow_error("BoundedString overflow");
        }
        std::memcpy(buffer_, sv.data(), l);
        buffer_[l] = '\0';
        len_ = l;
    }

    void assign(const char* data, std::size_t l) {
        ensure_bound();
        if (l >= cap_) {
            // throw std::overflow_error("BoundedString overflow");
        }
        std::memcpy(buffer_, data, l);
        buffer_[l] = '\0';
        len_ = l;
    }

    void append(std::string_view sv) {
        ensure_bound();
        const std::size_t l = sv.size();
        if (len_ + l >= cap_) {
            // throw std::overflow_error("BoundedString overflow");
        }
        std::memcpy(buffer_ + len_, sv.data(), l);
        len_ += l;
        buffer_[len_] = '\0';
    }

    void clear() noexcept {
        if (buffer_ && cap_) buffer_[0] = '\0';
        len_ = 0;
    }

    // accessors
    std::size_t size() const noexcept { return len_; }
    std::size_t capacity() const noexcept { return cap_ ? cap_ - 1 : 0; }
    std::size_t max_size() const noexcept { return capacity(); }
    const char* c_str() const noexcept { return buffer_; }
    char*       data() noexcept { return buffer_; }
    const char* data() const noexcept { return buffer_; }

    char& operator[](std::size_t i) noexcept { return buffer_[i]; }
    const char& operator[](std::size_t i) const noexcept { return buffer_[i]; }

private:
    void ensure_bound() const {
        if (!buffer_ || cap_ == 0) {
            // throw std::logic_error("BoundedString is not bound to a buffer");
        }
    }

    char*        buffer_;
    std::size_t  cap_;   // total bytes, including '\0'
    std::size_t  len_;   // number of chars excluding '\0'
};
