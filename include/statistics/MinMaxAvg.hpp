#pragma once
#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <math.h>
#include "time.hpp"

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
class MinMaxAvg
{
public:
    struct Stats
    {
        T min;
        T avg;
        T max;
        size_t count;
        static int toJson(const std::string_view name, const Stats &stats, std::span<char> json)
        {
            int written = snprintf(json.data(), json.size(), "{\"name\":\"%s\", \"value\":{\"min\":%f, \"avg\":%f, \"max\":%f, \"count\":%u}}",
                                name.data(), stats.min, stats.avg, stats.max, stats.count);
            if (written < 0 || static_cast<size_t>(written) >= json.size())
                json[written-1] = '\0';
            return written;
        }
    };
    MinMaxAvg() { reset(); }
    MinMaxAvg(T initialValue) : MinMaxAvg() { add(initialValue); }

    void reset()
    {
        stats = {0, 0, 0, 0};
        sum_ = (SumT)0;
    }

    inline void add(T v)
    {
        if constexpr (IGNORE_NAN && std::is_floating_point<T>::value)
        {
            if (isnan((double)v))
                return;
        }

        if (stats.count == 0)
        {
            stats.min = stats.max = v;
        }
        else
        {
            if (v < stats.min)
                stats.min = v;
            if (v > stats.max)
                stats.max = v;
        }
        sum_ += (SumT)v;
        ++stats.count;
    }

    inline void addMany(const T *data, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            add(data[i]);
    }

    // Queries
    inline bool hasData() const { return stats.count > 0; }
    inline uint32_t getCount() const { return stats.count; }
    inline T getMin() const { return stats.min; } // valid only if hasData()
    inline T getMax() const { return stats.max; } // valid only if hasData()
    inline SumT getSum() const { return sum_; }
    inline SumT getAvg() const { return stats.count ? (sum_ / (SumT)stats.count) : (SumT)0; }
    inline T getPeak() const { return hasData() ? (stats.max > -stats.min ? stats.max : stats.min) : (T)0; }
    inline T getPeakAbs() const { return hasData() ? (stats.max > -stats.min ? stats.max : -stats.min) : (T)0; }
    inline T getPeakToPeak() const { return hasData() ? (stats.max - stats.min) : (T)0; }
    inline T getMidRange() const { return hasData() ? ((stats.max + stats.min) / (T)2) : (T)0; }
    inline bool getRange(Stats &out) const
    {
        if (!hasData())
            return false;
        out.min = getMin();
        out.avg = getAvg();
        out.max = getMax();
        return true;
    }

    // Integer-rounded average (only when SumT is integral)
    template <typename U = SumT>
    inline typename std::enable_if<std::is_integral<U>::value, U>::type
    getAvgRounded() const
    {
        if (!stats.count)
            return (U)0;
        U c = (U)stats.count;
        U half = (sum_ >= 0 ? c / 2 : -(c / 2)); // round-to-nearest
        return (sum_ + half) / c;
    }

    // Fixed-point average: returns (avg * scale) rounded to nearest as OutT.
    // Example: scale=1000 → milli-units; scale=256 → Q8.8; scale=65536 → Q16.16.
    template <typename OutT>
    inline OutT getAvgFixed(OutT scale) const
    {
        if (!stats.count)
            return (OutT)0;
        long double avg = (long double)sum_ / (long double)stats.count;
        long double scaled = avg * (long double)scale;
        long double adj = (scaled >= 0) ? 0.5L : -0.5L;
        return (OutT)(scaled + adj);
    }

private:
    SumT sum_;
    Stats stats;
};

using rtos::time::Millis;

template <typename T, typename SumT = double, bool IGNORE_NAN = false>
class MinMaxAvgWindowed : public MinMaxAvg<T, SumT, IGNORE_NAN>
{
public:
    MinMaxAvgWindowed(Millis windowSize) : MinMaxAvg<T, SumT, IGNORE_NAN>(), windowSize_(windowSize)
    {
        reset();
    }

    void reset()
    {
        MinMaxAvg<T, SumT, IGNORE_NAN>::reset();
        startAddTime_ = Millis(0);
    }

    /// @brief Add a new sample with timestamp.
    /// @param v Sample value
    /// @param now Current time in milliseconds
    /// @return true if the window time has elapsed.
    inline bool add(T v, Millis now)
    {
        MinMaxAvg<T, SumT, IGNORE_NAN>::add(v);
        if (startAddTime_ == Millis(0))
        {
            startAddTime_ = now;
        }
        return (now - startAddTime_ > windowSize_);
    }

private:
    Millis windowSize_;
    Millis startAddTime_ = Millis(0);
};
