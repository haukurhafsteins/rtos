#pragma once
#include <stdexcept>
#include <cassert>
#include <cstddef>
#include <utility>

// Lock policies ---------------------------------------------------------------

struct NoLock {
    static inline void lock()   noexcept {}
    static inline void unlock() noexcept {}
};

template<typename L>
struct LockGuard {
    LockGuard()  noexcept { L::lock(); }
    ~LockGuard() noexcept { L::unlock(); }
};

// RingBuffer with lock policy -------------------------------------------------
// - FIFO push/pop by default (no overwrite)
// - push_overwrite() available when you deliberately want to drop oldest
// - External storage via initialize(), or use StaticRingBuffer below
// - Single-context by default; make it ISR-safe by supplying a Lock policy

template <typename T, typename LockPolicy = NoLock>
class RingBuffer {
public:
    RingBuffer() noexcept : _data(nullptr), _capacity(0), _head(0), _count(0) {}
    ~RingBuffer() = default;

    // Initialize with external storage (buffer_size in BYTES)
    void initialize(T* buffer, std::size_t buffer_size) noexcept {
        assert(buffer && buffer_size >= sizeof(T));
        _data = buffer;
        _capacity = buffer_size / sizeof(T);
        _head = 0;
        _count = 0;
    }

    // Reset indices (contents left as-is)
    void reset() noexcept {
        LockGuard<LockPolicy> g;
        _head = 0;
        _count = 0;
    }

    // FIFO push (no overwrite). Returns false if full.
    bool push(const T& value) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        if (isFullUnlocked()) return false;
        _data[_head] = value;
        _head = next(_head);
        ++_count;
        return true;
    }
    bool push(T&& value) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        if (isFullUnlocked()) return false;
        _data[_head] = std::move(value);
        _head = next(_head);
        ++_count;
        return true;
    }

    // Overwrite oldest when full (always succeeds)
    void push_overwrite(const T& value) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        _data[_head] = value;
        _head = next(_head);
        if (_count < _capacity) ++_count;
        // else keep _count == _capacity (oldest implicitly dropped)
    }
    void push_overwrite(T&& value) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        _data[_head] = std::move(value);
        _head = next(_head);
        if (_count < _capacity) ++_count;
    }

    // FIFO pop. Returns false if empty.
    bool pop(T& out) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        if (_count == 0) return false;
        const std::size_t tail = oldestIndexUnlocked();
        out = std::move(_data[tail]);
        --_count;
        return true;
    }

    // Pop up to max items into out[]. Returns number popped.
    std::size_t pop_n(T* out, std::size_t max) noexcept {
        LockGuard<LockPolicy> g;
        assert(_capacity && _data);
        if (_count == 0 || max == 0) return 0;
        std::size_t n = (_count < max) ? _count : max;

        std::size_t tail = oldestIndexUnlocked();
        std::size_t first = n;
        std::size_t till_end = _capacity - tail;
        if (first > till_end) first = till_end;

        for (std::size_t i = 0; i < first; ++i) out[i] = std::move(_data[tail + i]);

        const std::size_t second = n - first;
        for (std::size_t i = 0; i < second; ++i) out[first + i] = std::move(_data[i]);

        _count -= n;
        return n;
    }

    // Peek a contiguous span without popping; returns {ptr,len}
    // Span may be shorter than size() if wrapped.
    std::pair<const T*, std::size_t> peek_span() const noexcept {
        // read-only: no lock by default; add LockGuard if your platform needs it
        if (_count == 0) return {nullptr, 0};
        const std::size_t tail = oldestIndexUnlocked();
        if (_head > tail) return {&_data[tail], _head - tail};
        return {&_data[tail], _capacity - tail};
    }

    // Status
    bool isEmpty() const noexcept { return _count == 0; }
    bool isFull()  const noexcept { return _count == _capacity; }
    std::size_t size() const noexcept { return _count; }
    std::size_t capacity() const noexcept { return _capacity; }

    // Oldest-first, bounds-checked random access (0..size()-1)
    T& operator[](std::size_t i) {
        if (i >= _count) throw std::out_of_range("RingBuffer index");
        return _data[(oldestIndexUnlocked() + i) % _capacity];
    }
    const T& operator[](std::size_t i) const {
        if (i >= _count) throw std::out_of_range("RingBuffer index");
        return _data[(oldestIndexUnlocked() + i) % _capacity];
    }

    // Recent access (0 = most recent). Wraps around if idx >= size()
    const T& getRecent(std::size_t idx) const {
        // If idx >= _count, wrap around by capacity
        const std::size_t pos = (_head + _capacity - 1 - idx) % _capacity;
        return _data[pos];
    }
    void setRecent(std::size_t idx, const T& value) {
        LockGuard<LockPolicy> g;
        const std::size_t pos = (_head + _capacity - 1 - idx) % _capacity;
        _data[pos] = value;
    }

    // Absolute access (wraps by capacity)
    T&       getAt(std::size_t idx)       noexcept { return _data[idx % _capacity]; }
    const T& getAt(std::size_t idx) const noexcept { return _data[idx % _capacity]; }
    void     setAt(std::size_t idx, const T& v) noexcept {
        LockGuard<LockPolicy> g; _data[idx % _capacity] = v;
    }
    T*       getPointerAt(std::size_t idx) noexcept { return &_data[idx % _capacity]; }

    // Oldest index (where pop will read next)
    std::size_t oldestIndex() const noexcept {
        return oldestIndexUnlocked();
    }

    // Last appended element (unchecked; asserts when empty)
    T&       getLast()       { assert(_count); return _data[(_head + _capacity - 1) % _capacity]; }
    const T& getLast() const { assert(_count); return _data[(_head + _capacity - 1) % _capacity]; }

    // Introspection
    std::size_t headIndex()   const noexcept { return _head; }
    T*          data()        const noexcept { return _data; }
    std::size_t bufferBytes() const noexcept { return _capacity * sizeof(T); }
    std::size_t elementsBytes() const noexcept { return _count * sizeof(T); }
    std::size_t elements() const noexcept { return _count; }

    int toJson(std::span<char> buf, std::size_t count, const std::string& format = "%.6g") const {
        LockGuard<LockPolicy> g;
        if (count > _count)
            return -1;

        size_t offset = 0;
        offset += snprintf(buf.data() + offset, buf.size() - offset, "[");

        // Start for newest to oldest
        for (size_t i = 0; i < count; i++)
        {
            const T& val = getRecent(count - 1 - i);
            offset += snprintf(buf.data() + offset, buf.size() - offset, "%.6g", static_cast<double>(val));
            if (i < count - 1)
                offset += snprintf(buf.data() + offset, buf.size() - offset, ",");
        }
        
        offset += snprintf(buf.data() + offset, buf.size() - offset, "]");
        return static_cast<int>(offset);
    }

private:
    // helpers
    std::size_t next(std::size_t i) const noexcept { return (i + 1) % _capacity; }
    bool isFullUnlocked() const noexcept { return _count == _capacity; }
    std::size_t oldestIndexUnlocked() const noexcept {
        return (_head + _capacity - _count) % _capacity; // head - count (mod capacity)
    }

    T*          _data;
    std::size_t _capacity; // elements
    std::size_t _head;     // write position
    std::size_t _count;    // number of valid elements
};

// Fixed-storage variant -------------------------------------------------------
// ElementCount is the number of elements (not bytes).
template <typename T, std::size_t ElementCount, typename LockPolicy = NoLock>
class StaticRingBuffer : public RingBuffer<T, LockPolicy> {
public:
    StaticRingBuffer() noexcept {
        RingBuffer<T, LockPolicy>::initialize(_storage, sizeof(_storage));
    }
private:
    T _storage[ElementCount];
};
