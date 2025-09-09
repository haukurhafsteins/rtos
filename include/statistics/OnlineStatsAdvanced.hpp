#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <cmath>
#include "MinMaxAvg.hpp"

// Advanced online stats with higher moments (M2..M4) and EMA/EWVAR.
// T must be floating-point. SumT is the internal high-precision accumulator.
// IGNORE_NAN: skip non-finite samples.

template <typename T, typename SumT = long double, bool IGNORE_NAN = true>
class OnlineStatsAdvanced {
    static_assert(std::is_floating_point<T>::value, "T must be floating-point");
public:
    OnlineStatsAdvanced() = default;

    // Set EMA smoothing factor in (0,1].
    void setAlpha(T a) {
        if (a <= (T)0) a = (T)0.0001;   // tiny but positive
        if (a >  (T)1) a = (T)1;
        alpha_ = a;
    }

    // Add one sample (non-finite values are ignored when IGNORE_NAN=true)
    inline void add(T x) {
        if constexpr (IGNORE_NAN) {
            if (!std::isfinite(x)) return;
        }
        // Min/Max/Count tracking
        mm_.add(x);

        // Higher moments (Pébay/West one-pass)
        const SumT xn = (SumT)x;
        const SumT n0 = (SumT)n_;
        const SumT n1 = n0 + 1;
        const SumT delta = xn - mean_;
        const SumT delta_n = delta / n1;
        const SumT delta_n2 = delta_n * delta_n;
        const SumT term1 = delta * delta_n * n0; // == (xn-mean_)*(xn-mean_new)*n0

        M4_ += term1 * delta_n2 * (n1*n1 - 3*n1 + 3) + (SumT)6.0 * delta_n2 * M2_ - (SumT)4.0 * delta_n * M3_;
        M3_ += term1 * delta_n * (n1 - 2) - (SumT)3.0 * delta_n * M2_;
        M2_ += term1;
        mean_ += delta_n;
        n_ += 1;

        // RMS accumulator
        sumSquares_ += xn * xn;

        // Geometric / Harmonic means (conditional updates)
        if (xn > (SumT)0) { sumLog_ += std::log(xn); n_log_++; }
        if (x != (T)0)     { sumInv_ += ((SumT)1) / xn; n_inv_++; }

        // EMA / EWVAR (West-style)
        if (!ema_init_) {
            ema_mean_ = x;
            ema_var_  = (T)0;
            ema_init_ = true;
        } else {
            const T m_prev = ema_mean_;
            ema_mean_ = (T)((1 - alpha_) * (SumT)m_prev + alpha_ * xn);
            // variance update that keeps positivity
            ema_var_  = (T)((1 - alpha_) * ((SumT)ema_var_ + alpha_ * (xn - (SumT)m_prev) * (xn - (SumT)ema_mean_)));
        }
    }

    // Resets all aggregates
    void reset() {
        n_ = n_log_ = n_inv_ = 0;
        mean_ = M2_ = M3_ = M4_ = sumSquares_ = sumLog_ = sumInv_ = (SumT)0;
        ema_mean_ = ema_var_ = (T)0; ema_init_ = false; alpha_ = (T)0.1;
        mm_.reset();
    }

    // --- Basic stats ---
    inline std::uint64_t count() const { return n_; }
    inline bool hasData() const { return n_ > 0; }

    inline T mean() const { return (T)mean_; }

    inline T variance_population() const { return n_ ? (T)(M2_ / (SumT)n_) : (T)0; }
    inline T variance_sample() const { return n_ > 1 ? (T)(M2_ / (SumT)(n_ - 1)) : (T)0; }

    inline T stddev_population() const { T v = variance_population(); return v > (T)0 ? (T)std::sqrt((double)v) : (T)0; }
    inline T stddev_sample() const { T v = variance_sample(); return v > (T)0 ? (T)std::sqrt((double)v) : (T)0; }

    inline T rms() const { return n_ ? (T)std::sqrt((double)(sumSquares_ / (SumT)n_)) : (T)0; }
    inline T ac_rms_population() const { return stddev_population(); } // RMS of (x - mean)

    inline T cv_population() const { T mu = mean(); T s = stddev_population(); return (mu != (T)0) ? (T)(s / std::fabs((double)mu)) : (T)0; }

    // --- Higher-order moments ---
    inline T skewness_population() const {
        if (n_ < 2 || M2_ == (SumT)0) return (T)0;
        // g1 = sqrt(n) * M3 / M2^(3/2)
        return (T)((std::sqrt((long double)n_) * M3_) / std::pow(M2_, (long double)1.5));
    }
    inline T skewness_unbiased() const {
        if (n_ < 3) return (T)0;
        T g1 = skewness_population();
        long double n = (long double)n_;
        return (T)(std::sqrt(n * (n - 1)) / (n - 2) * (long double)g1);
    }

    inline T kurtosis_excess_population() const {
        if (n_ < 2 || M2_ == (SumT)0) return (T)0;
        // g2 = (n*M4)/(M2^2) - 3
        return (T)(((SumT)n_ * M4_) / (M2_ * M2_) - (SumT)3);
    }
    inline T kurtosis_excess_unbiased() const {
        if (n_ < 4) return (T)0;
        long double n = (long double)n_;
        long double g2 = ( (n * (long double)M4_) / ((long double)M2_ * (long double)M2_) ) - 3.0L;
        return (T)(((n - 1) / ((n - 2) * (n - 3))) * ((n + 1) * g2 + 6));
    }

    // --- Peaks & range (delegated to MinMaxAvg) ---
    inline T min() const { return mm_.hasData() ? mm_.getMin() : (T)0; }
    inline T max() const { return mm_.hasData() ? mm_.getMax() : (T)0; }
    inline T peak_abs() const { return mm_.hasData() ? mm_.getPeakAbs() : (T)0; }
    inline T peak_to_peak() const { return mm_.hasData() ? mm_.getPeakToPeak() : (T)0; }
    inline T peak_from_mean_final() const {
        if (!mm_.hasData()) return (T)0;
        const T mu = mean();
        const T d1 = (T)std::fabs((double)(mu - min()));
        const T d2 = (T)std::fabs((double)(max() - mu));
        return d1 > d2 ? d1 : d2;
    }

    // --- Geometric & Harmonic means ---
    inline T geometric_mean() const { return n_log_ ? (T)std::exp((double)(sumLog_ / (SumT)n_log_)) : (T)0; }
    inline T harmonic_mean() const { return (n_inv_ && sumInv_ != (SumT)0) ? (T)((SumT)n_inv_ / sumInv_) : (T)0; }

    // --- EMA accessors ---
    inline bool ema_ready() const { return ema_init_; }
    inline T ema_mean() const { return ema_init_ ? ema_mean_ : (T)0; }
    inline T ewvar() const { return ema_init_ ? ema_var_ : (T)0; }
    inline T ewstd() const { return (ema_init_ && ema_var_ > (T)0) ? (T)std::sqrt((double)ema_var_) : (T)0; }

private:
    // exact aggregates
    std::uint64_t n_ = 0, n_log_ = 0, n_inv_ = 0;
    SumT mean_ = 0, M2_ = 0, M3_ = 0, M4_ = 0, sumSquares_ = 0, sumLog_ = 0, sumInv_ = 0;

    // EMA/EWVAR
    T alpha_ = (T)0.1; // default smoothing
    T ema_mean_ = (T)0, ema_var_ = (T)0; bool ema_init_ = false;

    // Range tracking
    MinMaxAvg<T, SumT, IGNORE_NAN> mm_;
};
