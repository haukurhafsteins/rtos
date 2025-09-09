#pragma once
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <limits>
#include "MinMaxAvg.hpp"

template <typename T>
class OnlineStatistics
{
    static_assert(std::is_floating_point<T>::value, "OnlineStatistics requires a floating point type.");

public:
    OnlineStatistics()
    {
        reset();
    }

    // Add a new sample
    void add(T x)
    {
        if (!std::isfinite(x)) return;
        
        minMaxAvg.add(x);

        // Update mean and M2 (Welford's algorithm)
        T delta = x - mean_;
        mean_ += delta / static_cast<T>(minMaxAvg.getCount());
        T delta2 = x - mean_;
        M2_ += delta * delta2;

        // Update RMS accumulator
        sum_squares_ += x * x;

        // Update peak from mean (max absolute deviation from mean)
        T abs_deviation = std::abs(x - mean_);
        if (abs_deviation > peak_from_mean_)
        {
            peak_from_mean_ = abs_deviation;
        }
    }

    // Reset all internal state
    void reset()
    {
        minMaxAvg.reset();
        mean_ = static_cast<T>(0);
        M2_ = static_cast<T>(0);
        sum_squares_ = static_cast<T>(0);
        peak_from_mean_ = static_cast<T>(0);
    }

    // Mean of values
    T mean() const { return mean_; }

    // Variance (sample, n - 1)
    T variance() const
    {
        auto n = minMaxAvg.getCount();
        return (n > 1) ? M2_ / static_cast<T>(n - 1) : static_cast<T>(0);
    }

    // Standard deviation
    T stddev() const { return std::sqrt(variance()); }

    // Root mean square
    T rms() const
    {
        return (minMaxAvg.hasData()) ? std::sqrt(sum_squares_ / static_cast<T>(minMaxAvg.getCount())) : static_cast<T>(0);
    }

    // Maximum absolute value (peak)
    T peak() const { return minMaxAvg.getPeakAbs(); }

    // Maximum absolute deviation from mean (peak from mean)
    T peak_from_mean() const
    {
        if (!minMaxAvg.hasData())
            return static_cast<T>(0);
        T mu = mean_;
        T d1 = std::abs(mu - min());
        T d2 = std::abs(max() - mu);
        return d1 > d2 ? d1 : d2;
    }

    // Number of samples
    std::size_t count() const { return static_cast<std::size_t>(minMaxAvg.getCount()); }
    // At least 2 samples?
    bool has_variance() const { return minMaxAvg.getCount() > 1; }

    T min() const { return (minMaxAvg.hasData()) ? minMaxAvg.getMin() : static_cast<T>(0); }
    T max() const { return (minMaxAvg.hasData()) ? minMaxAvg.getMax() : static_cast<T>(0); }
    T peak_to_peak() const { return (minMaxAvg.hasData()) ? (minMaxAvg.getMax() - minMaxAvg.getMin()) : static_cast<T>(0); }

    void print() const
    {
        std::size_t n = count();
        std::printf("OnlineStatistics:\n");
        std::printf("    count         : %zu\n", n);
        std::printf("    mean          : %.6f\n", (double)mean());
        std::printf("    variance      : %.6f\n", (double)variance());
        std::printf("    stddev        : %.6f\n", (double)stddev());
        std::printf("    rms           : %.6f\n", (double)rms());
        std::printf("    peak          : %.6f\n", (double)peak());
        std::printf("    peak from mean: %.6f\n", (double)peak_from_mean());
        std::printf("    min           : %.6f\n", (double)min());
        std::printf("    max           : %.6f\n", (double)max());
        std::printf("    peak to peak  : %.6f\n", (double)peak_to_peak());
    }

private:
    MinMaxAvg<T> minMaxAvg;
    T mean_ = static_cast<T>(0);
    T M2_ = static_cast<T>(0);
    T sum_squares_ = static_cast<T>(0);
    T peak_from_mean_ = static_cast<T>(0);
};
