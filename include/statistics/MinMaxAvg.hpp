#pragma once
#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <math.h>

// Embedded-friendly Min/Max/Average
// - No dynamic allocation, tiny headers
// - O(1) per add()
// - Optional NaN filtering for floating inputs
// - Fixed-point average helper
//
// Template params:
//   T          : input/sample type (e.g., int16_t, int32_t, float)
//   SumT       : accumulator type for sum (default = double). Use a wider type
//                for integers (e.g., int64_t) to avoid overflow.
//   IGNORE_NAN : if true and T is floating, NaNs are skipped silently.
template <typename T, typename SumT = double, bool IGNORE_NAN = false>
class MinMaxAvg {
public:
    MinMaxAvg() { reset(); }
    MinMaxAvg(T initialValue) : MinMaxAvg() { add(initialValue); }

    void reset() {
        count_ = 0;
        sum_   = (SumT)0;
    }

    inline void add(T v) {
        if constexpr (IGNORE_NAN && std::is_floating_point<T>::value) {
            if (isnan((double)v)) return;
        }

        if (count_ == 0) {
            min_ = max_ = v;
        } else {
            if (v < min_) min_ = v;
            if (v > max_) max_ = v;
        }
        sum_ += (SumT)v;
        ++count_;
    }

    inline void addMany(const T* data, size_t n) {
        for (size_t i = 0; i < n; ++i) add(data[i]);
    }

    // Queries
    inline bool     hasData() const  { return count_ > 0; }
    inline uint32_t getCount() const { return count_; }
    inline T        getMin()   const { return min_; } // valid only if hasData()
    inline T        getMax()   const { return max_; } // valid only if hasData()
    inline SumT     getSum()   const { return sum_; }
    inline SumT     getAvg()   const { return count_ ? (sum_ / (SumT)count_) : (SumT)0; }
    inline T        getPeak()  const { return hasData() ? (max_ > -min_ ? max_ : min_) : (T)0; }
    inline T        getPeakAbs() const { return hasData() ? (max_ > -min_ ? max_ : -min_) : (T)0; }
    inline T        getPeakToPeak() const { return hasData() ? (max_ - min_) : (T)0; }
    inline T        getMidRange() const { return hasData() ? ((max_ + min_) / (T)2) : (T)0; } 

    // Integer-rounded average (only when SumT is integral)
    template <typename U = SumT>
    inline typename std::enable_if<std::is_integral<U>::value, U>::type
    getAvgRounded() const {
        if (!count_) return (U)0;
        U c = (U)count_;
        U half = (sum_ >= 0 ? c / 2 : -(c / 2));   // round-to-nearest
        return (sum_ + half) / c;
    }

    // Fixed-point average: returns (avg * scale) rounded to nearest as OutT.
    // Example: scale=1000 → milli-units; scale=256 → Q8.8; scale=65536 → Q16.16.
    template <typename OutT>
    inline OutT getAvgFixed(OutT scale) const {
        if (!count_) return (OutT)0;
        long double avg = (long double)sum_ / (long double)count_;
        long double scaled = avg * (long double)scale;
        long double adj = (scaled >= 0) ? 0.5L : -0.5L;
        return (OutT)(scaled + adj);
    }

private:
    uint32_t count_;
    SumT     sum_;
    T        min_;
    T        max_;
};
