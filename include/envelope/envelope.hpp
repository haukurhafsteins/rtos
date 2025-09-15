// envelope.hpp — header-only minimal envelope/limit checker
// SPDX-License-Identifier: MIT
//
// Goals
// - Tiny, zero-alloc, no exceptions/RTTI
// - Per-rule timers (enter/exit delays) to debounce violations
// - Fixed priority: first bound that violates wins
// - Works with value types T (float, double, ints, etc.) and time types
//   where Time::rep is either a floating-point (seconds) or an unsigned integer (ticks)
// - Compile with C++14 or later (uses <array>, no std::optional)
//
// Usage (outline)
//   using Seconds = envelope::SecondsTime; // or define your own Time policy
//   envelope::Below<float, Seconds> below0 { .hi = 0.0f, .enter_delay = 0.05f };
//   envelope::Within<float, Seconds> inside { .lo = 10.0f, .hi = 20.0f };
//   envelope::Above<float, Seconds> above  { .lo = 100.0f, .enter_delay = 0.50f };
//   envelope::Envelope<float, 3, Seconds> env;
//   env.bind(0, below0);
//   env.bind(1, inside);
//   env.bind(2, above);
//   auto r = env.update(sensor_value, now_seconds);
//   if (r.state == envelope::State::Violation) { /* handle rule r.index */ }
//
#pragma once

#include <cstdint>
#include <array>
#include <type_traits>
#include <limits>
#include <tuple>
#include <cstddef>

namespace envelope {

// ---------------- Common types ----------------

enum class State : std::uint8_t { Normal = 0, Violation = 1 };

struct Result {
    constexpr static uint8_t NO_VIOLATION = 0xFF; 
    State state{State::Normal};
    std::uint8_t index{NO_VIOLATION}; // 0xFF means "no violation"
};

// ---------------- Time policies ----------------
// Provide a Time policy with a member alias `using rep = <time_representation>`.
// `rep` must be either a floating point type (seconds) or an *unsigned* integer (ticks).

/// @brief Time policy with floating-point seconds representation.
struct SecondsTime { using rep = float; };

// Helper to ensure Time::rep is valid.
template<class Rep>
struct time_traits {
    static constexpr bool ok = std::is_floating_point<Rep>::value ||
                               (std::is_integral<Rep>::value && std::is_unsigned<Rep>::value);
};

// Elapsed time computation. For unsigned integer tick counters, wrap-around-safe via modular arithmetic.
// For floating point, normal subtraction.

template<class Rep>
inline Rep elapsed(Rep now, Rep since) {
    static_assert(time_traits<Rep>::ok, "Time::rep must be floating-point or unsigned integer");
    return static_cast<Rep>(now - since); // unsigned wraps mod 2^N, which is intended
}

// ---------------- Boundary policy ----------------
// Inclusive=false => strict inequalities; Inclusive=true => inclusive at edges.

template<bool Inclusive>
struct Bounds {
    template<class T>
    static inline bool below_ok(const T& v, const T& hi) {
        return Inclusive ? (v <= hi) : (v < hi);
    }
    template<class T>
    static inline bool above_ok(const T& v, const T& lo) {
        return Inclusive ? (v >= lo) : (v > lo);
    }
    template<class T>
    static inline bool inside_ok(const T& v, const T& lo, const T& hi) {
        return above_ok(v, lo) && below_ok(v, hi);
    }
};

// ---------------- Base rule with enter/exit delays ----------------
// Pattern: compute an immediate condition `is_violation_now(value)` in derived class;
// the base then debounces with enter/exit delays.

template<class Time>
struct Debounce {
    using rep = typename Time::rep;

    // Debounce configuration
    rep enter_delay{0}; // time to *enter* violation after immediate condition becomes true
    rep exit_delay{0};  // time to *exit* violation after immediate condition becomes false

    // Internal state
    bool violating{false};
    bool enter_pending{false};
    bool exit_pending{false};
    rep  enter_since{}; // valid iff enter_pending
    rep  exit_since{};  // valid iff exit_pending

    inline void reset() {
        violating = false;
        enter_pending = exit_pending = false;
        enter_since = exit_since = rep{};
    }

protected:
    // Returns true when the *stable* output should be "violation"
    bool step(bool cond_now, typename Time::rep now) {
        if (cond_now) {
            // moving toward violation
            exit_pending = false;
            if (violating) return true; // already in violation
            if (!enter_pending) { enter_pending = true; enter_since = now; }
            if (elapsed(now, enter_since) >= enter_delay) {
                enter_pending = false; violating = true; return true;
            }
            return false; // waiting to enter violation
        } else {
            // moving toward normal
            enter_pending = false;
            if (!violating) return false; // already normal
            if (!exit_pending) { exit_pending = true; exit_since = now; }
            if (elapsed(now, exit_since) >= exit_delay) {
                exit_pending = false; violating = false; return false;
            }
            return true; // waiting to exit violation
        }
    }
};

// ---------------- Rule kinds ----------------
// Conventions:
// - Each rule exposes: fields for thresholds, Debounce<Time> base for delays/state,
//   operator()(T value, Time::rep now) -> bool (true == violation)
// - Bounds policy B selects inclusivity at edges (default: exclusive)

// Violation when value > hi
// Normal when value <= hi

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct Above : Debounce<Time> {
    T hi{}; // upper bound (max acceptable)
    inline bool operator()(const T& v, typename Time::rep now) const {
        // immediate violation if v > hi
        bool cond = !B::below_ok(v, hi);
        // const_cast: allow mutable debounce state without giving up const operator()
        return const_cast<Above*>(this)->Debounce<Time>::step(cond, now);
    }
};

// Violation when value < lo
// Normal when value >= lo

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct Below : Debounce<Time> {
    T lo{}; // lower bound (min acceptable)
    inline bool operator()(const T& v, typename Time::rep now) const {
        bool cond = !B::above_ok(v, lo);
        return const_cast<Below*>(this)->Debounce<Time>::step(cond, now);
    }
};

// Keep value within [lo, hi]
// Violation when outside the band

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct Within : Debounce<Time> {
    T lo{};
    T hi{};
    inline bool operator()(const T& v, typename Time::rep now) const {
        bool cond = !B::inside_ok(v, lo, hi); // true if outside -> violation tendency
        return const_cast<Within*>(this)->Debounce<Time>::step(cond, now);
    }
};

// Keep value outside [lo, hi] (forbidden band)
// Violation when inside the band

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct Outside : Debounce<Time> {
    T lo{};
    T hi{};
    inline bool operator()(const T& v, typename Time::rep now) const {
        bool cond = B::inside_ok(v, lo, hi); // true if inside -> violation tendency
        return const_cast<Outside*>(this)->Debounce<Time>::step(cond, now);
    }
};

// ----- Hysteresis variants -----
// WithinHysteresis: normal when inside [lo_exit, hi_exit] (inner band)
// Enter violation when value goes outside [lo_enter, hi_enter] (outer band)
// The thresholds should satisfy: lo_enter <= lo_exit <= hi_exit <= hi_enter

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct WithinHysteresis : Debounce<Time> {
    T lo_enter{}; T hi_enter{}; // outer thresholds for entering violation
    T lo_exit{};  T hi_exit{};  // inner thresholds for exiting violation
    inline bool operator()(const T& v, typename Time::rep now) const {
        auto* self = const_cast<WithinHysteresis*>(this);
        bool cond;
        if (self->violating) {
            // while violating, stay violating until we are back inside the inner band
            bool back_inside = B::inside_ok(v, lo_exit, hi_exit);
            cond = !back_inside; // if not back inside, keep violation tendency
        } else {
            // while normal, start violation only if we cross outer thresholds
            bool outside_outer = !B::inside_ok(v, lo_enter, hi_enter);
            cond = outside_outer;
        }
        return self->Debounce<Time>::step(cond, now);
    }
};

// OutsideHysteresis: normal when outside [lo_exit, hi_exit] (outer band)
// Enter violation when value goes inside [lo_enter, hi_enter] (inner band)
// The thresholds should satisfy: lo_exit <= lo_enter <= hi_enter <= hi_exit

template<class T, class Time = SecondsTime, class B = Bounds<false> >
struct OutsideHysteresis : Debounce<Time> {
    T lo_enter{}; T hi_enter{}; // inner thresholds to enter violation
    T lo_exit{};  T hi_exit{};  // outer thresholds to exit violation
    inline bool operator()(const T& v, typename Time::rep now) const {
        auto* self = const_cast<OutsideHysteresis*>(this);
        bool cond;
        if (self->violating) {
            // while violating (inside band), remain violating until we leave the OUTER band
            bool outside_outer = !B::inside_ok(v, lo_exit, hi_exit);
            cond = !outside_outer; // stay violating while still inside outer band
        } else {
            // while normal (outside), enter violation if we move into the INNER band
            bool inside_inner = B::inside_ok(v, lo_enter, hi_enter);
            cond = inside_inner;
        }
        return self->Debounce<Time>::step(cond, now);
    }
};

// ---------------- Aggregator ----------------
// Fixed priority order: rule at index 0 has highest priority.

// type-erased callable signature

template<class T, class Time>
using rule_fn = bool (*)(const void* self, const T& v, typename Time::rep now);

// stub that casts and invokes a concrete Rule type

template<class Rule, class T, class Time>
static bool call_rule(const void* self, const T& v, typename Time::rep now) {
    return (*static_cast<const Rule*>(self))(v, now);
}

// Envelope holds pointers to rule objects you create elsewhere (no ownership)

template<class T, std::size_t N, class Time = SecondsTime>
struct Envelope {
    static_assert(N > 0, "Envelope must have at least one rule");
    using rep = typename Time::rep;

    std::array< rule_fn<T,Time>, N > vtbl{};
    std::array< const void*, N >     rules{};

    // Bind a rule object at position i. The rule must outlive the Envelope.
    template<class Rule>
    inline void bind(std::size_t i, const Rule& r) {
        vtbl[i]  = &call_rule<Rule, T, Time>;
        rules[i] = &r;
    }

    // Evaluate rules in order and return first violation, or Normal if none.
    inline Result update(const T& value, rep now) const {
        for (std::size_t i = 0; i < N; ++i) {
            if (vtbl[i] && rules[i]) {
                if (vtbl[i](rules[i], value, now)) {
                    Result res; res.state = State::Violation; res.index = static_cast<std::uint8_t>(i);
                    return res;
                }
            }
        }
        return Result{}; // Normal
    }
};

// ----- Compile-time typed aggregator (no function pointers) -----

template<class T, class Time, class... RuleTypes>
struct EnvelopeT {
    static_assert(sizeof...(RuleTypes) > 0, "EnvelopeT must have at least one rule");
    using rep = typename Time::rep;

    std::tuple<RuleTypes*...> rules; // stores pointers to external rule objects

    explicit EnvelopeT(RuleTypes&... rs) : rules{ &rs... } {}

    // recursion helper for C++14
    template<std::size_t I, std::size_t N>
    struct Runner {
        static inline Result run(const std::tuple<RuleTypes*...>& t, const T& v, rep now) {
            auto* r = std::get<I>(t);
            if (r && (*r)(v, now)) {
                Result res; res.state = State::Violation; res.index = static_cast<std::uint8_t>(I); return res;
            }
            return Runner<I+1, N>::run(t, v, now);
        }
    };
    template<std::size_t N>
    struct Runner<N, N> {
        static inline Result run(const std::tuple<RuleTypes*...>&, const T&, rep) { return Result{}; }
    };

    inline Result update(const T& value, rep now) const {
        return Runner<0, sizeof...(RuleTypes)>::run(rules, value, now);
    }

    // reset all bound rules (requires they expose reset())
    template<std::size_t I, std::size_t N>
    struct Resetter {
        static inline void apply(std::tuple<RuleTypes*...>& t) {
            auto* r = std::get<I>(t);
            if (r) r->reset();
            Resetter<I+1, N>::apply(t);
        }
    };
    template<std::size_t N>
    struct Resetter<N, N> { static inline void apply(std::tuple<RuleTypes*...>&) {} };

    inline void reset_all() { Resetter<0, sizeof...(RuleTypes)>::apply(rules); }
};

// helper factory to deduce template args
template<class T, class Time, class... Rules>
inline EnvelopeT<T,Time,Rules...> make_envelopeT(Rules&... rs) { return EnvelopeT<T,Time,Rules...>(rs...); }

// ---------------- Utilities ----------------


// Helpers to get +/-infinity for a given numeric T, when available

template<class T>
inline T pos_inf() {
    return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}

template<class T>
inline T neg_inf() {
    return std::numeric_limits<T>::has_infinity
         ? -std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::lowest();
}

// Preset builders (return rule objects with common semantics)

// NORMAL_BELOW(th): normal when v <= th; violation when v > th

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline Above<T,Time,B> NORMAL_BELOW(T th, typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    Above<T,Time,B> r; r.hi = th; r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}

// NORMAL_ABOVE(th): normal when v >= th; violation when v < th

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline Below<T,Time,B> NORMAL_ABOVE(T th, typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    Below<T,Time,B> r; r.lo = th; r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}

// NORMAL_WITHIN(lo, hi): normal when lo <= v <= hi

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline Within<T,Time,B> NORMAL_WITHIN(T lo, T hi, typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    Within<T,Time,B> r; r.lo = lo; r.hi = hi; r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}

// NORMAL_OUTSIDE(lo, hi): normal when v < lo or v > hi (forbidden band [lo,hi])

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline Outside<T,Time,B> NORMAL_OUTSIDE(T lo, T hi, typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    Outside<T,Time,B> r; r.lo = lo; r.hi = hi; r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}

// Hysteresis presets
// NORMAL_WITHIN_HYST: normal when inside [lo_exit, hi_exit]; violation when outside [lo_enter, hi_enter]

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline WithinHysteresis<T,Time,B> NORMAL_WITHIN_HYST(T lo_enter, T lo_exit, T hi_exit, T hi_enter,
                                                     typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    WithinHysteresis<T,Time,B> r; r.lo_enter = lo_enter; r.hi_enter = hi_enter; r.lo_exit = lo_exit; r.hi_exit = hi_exit;
    r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}

// NORMAL_OUTSIDE_HYST: normal when outside [lo_exit, hi_exit]; violation when inside [lo_enter, hi_enter]

template<class T, class Time = SecondsTime, class B = Bounds<false> >
inline OutsideHysteresis<T,Time,B> NORMAL_OUTSIDE_HYST(T lo_exit, T lo_enter, T hi_enter, T hi_exit,
                                                       typename Time::rep enter_delay = 0, typename Time::rep exit_delay = 0) {
    OutsideHysteresis<T,Time,B> r; r.lo_enter = lo_enter; r.hi_enter = hi_enter; r.lo_exit = lo_exit; r.hi_exit = hi_exit;
    r.enter_delay = enter_delay; r.exit_delay = exit_delay; return r;
}


// ---------------- Array support ----------------
// Apply the same rule(s) across an array of values with per-element debounce state.
// Two wrappers are provided:
//   - PerElement<Rule, N>: compile-time size, stores N independent Rule instances
//   - PerElementDyn<Rule, MaxN>: fixed-capacity, runtime length <= MaxN
// Reducers decide how to turn per-element violations into a single array result.

struct ArrayResult {
    State state{State::Normal};           // Normal or Violation
    std::uint8_t rule_index{0xFF};        // which rule triggered (priority order)
    std::uint16_t first_index{0xFFFF};    // first offending element (if applicable)
    std::uint16_t count{0};               // number of offending elements or run length
};

// --- Per-element rule containers ---

template<class Rule, std::size_t N>
struct PerElement {
    std::array<Rule, N> r;
    explicit PerElement(const Rule& proto = Rule{}) { r.fill(proto); }
    inline Rule& operator[](std::size_t i)             { return r[i]; }
    inline const Rule& operator[](std::size_t i) const { return r[i]; }
    inline void reset_all() { for (auto& x : r) x.reset(); }
};

template<class Rule, std::size_t MaxN>
struct PerElementDyn {
    std::array<Rule, MaxN> r;
    std::size_t n{MaxN};
    explicit PerElementDyn(const Rule& proto = Rule{}, std::size_t n_ = MaxN) : n(n_) { r.fill(proto); }
    inline void set_size(std::size_t n_) { n = (n_ <= MaxN) ? n_ : MaxN; }
    inline std::size_t size() const { return n; }
    inline Rule& operator[](std::size_t i)             { return r[i]; }
    inline const Rule& operator[](std::size_t i) const { return r[i]; }
    inline void reset_all() { for (std::size_t i = 0; i < n; ++i) r[i].reset(); }
};

// --- Reducers ---
// Each reducer exposes: static bool eval(pe, values, now, first, count)
// returning true if the array as a whole should be considered a violation for that rule.

struct AnyElement {
    template<class RulePE, class T, std::size_t N, class Time>
    static inline bool eval(const RulePE& pe, const T (&vals)[N], typename Time::rep now,
                            std::uint16_t& first, std::uint16_t& count) {
        count = 0; first = 0xFFFF;
        for (std::size_t i = 0; i < N; ++i) {
            if (pe.r[i](vals[i], now)) { first = static_cast<std::uint16_t>(i); count = 1; return true; }
        }
        return false;
    }
};

template<std::size_t K>
struct CountAtLeast {
    template<class RulePE, class T, std::size_t N, class Time>
    static inline bool eval(const RulePE& pe, const T (&vals)[N], typename Time::rep now,
                            std::uint16_t& first, std::uint16_t& count) {
        first = 0xFFFF; count = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (pe.r[i](vals[i], now)) { if (first == 0xFFFF) first = static_cast<std::uint16_t>(i); if (++count >= K) return true; }
        }
        return false;
    }
};

template<std::size_t Num, std::size_t Den>
struct FractionAtLeast {
    static_assert(Den > 0, "Denominator must be > 0");
    template<class RulePE, class T, std::size_t N, class Time>
    static inline bool eval(const RulePE& pe, const T (&vals)[N], typename Time::rep now,
                            std::uint16_t& first, std::uint16_t& count) {
        first = 0xFFFF; count = 0;
        const std::size_t required = (Num * N + (Den - 1)) / Den; // ceil(Num/Den * N)
        for (std::size_t i = 0; i < N; ++i) {
            if (pe.r[i](vals[i], now)) {
                if (first == 0xFFFF) first = static_cast<std::uint16_t>(i);
                if (++count >= required) return true;
            }
        }
        return false;
    }
};

template<std::size_t L>
struct RunLengthAtLeast {
    template<class RulePE, class T, std::size_t N, class Time>
    static inline bool eval(const RulePE& pe, const T (&vals)[N], typename Time::rep now,
                            std::uint16_t& first, std::uint16_t& count) {
        first = 0xFFFF; count = 0;
        std::size_t run = 0; std::size_t run_start = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (pe.r[i](vals[i], now)) {
                if (run == 0) run_start = i;
                if (++run >= L) { first = static_cast<std::uint16_t>(run_start); count = static_cast<std::uint16_t>(run); return true; }
            } else {
                run = 0;
            }
        }
        return false;
    }
};

struct AllElements {
    template<class RulePE, class T, std::size_t N, class Time>
    static inline bool eval(const RulePE& pe, const T (&vals)[N], typename Time::rep now,
                            std::uint16_t& first, std::uint16_t& count) {
        first = 0xFFFF; count = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (!pe.r[i](vals[i], now)) return false;
            if (first == 0xFFFF) first = static_cast<std::uint16_t>(i);
            ++count;
        }
        return true; // all violated
    }
};

// ----- Array aggregator (typed, no function pointers) -----
// Usage:
//   using R0 = PerElement<Within<float,Seconds>, N>;
//   using R1 = PerElement<Above<float,Seconds>,  N>;
//   R0 r0(proto_within); R1 r1(proto_above);
//   EnvelopeArrayT<float, N, Seconds, AnyElement, R0, R1> env(r0, r1);
//   ArrayResult ar = env.update(values, now);

template<class T, std::size_t N, class Time, class Reduce, class... RulePEs>
struct EnvelopeArrayT {
    using rep = typename Time::rep;
    std::tuple<RulePEs*...> rules;
    explicit EnvelopeArrayT(RulePEs&... rs) : rules{ &rs... } {}

    template<std::size_t I, std::size_t M>
    struct Runner {
        static inline ArrayResult run(const std::tuple<RulePEs*...>& t, const T (&vals)[N], rep now) {
            auto* r = std::get<I>(t);
            if (r) {
                std::uint16_t first, count;
                if (Reduce::template eval<typename std::remove_reference<decltype(*r)>::type, T, N, Time>(*r, vals, now, first, count)) {
                    ArrayResult out; out.state = State::Violation; out.rule_index = static_cast<std::uint8_t>(I);
                    out.first_index = first; out.count = count; return out;
                }
            }
            return Runner<I+1, M>::run(t, vals, now);
        }
    };
    template<std::size_t M>
    struct Runner<M, M> {
        static inline ArrayResult run(const std::tuple<RulePEs*...>&, const T (&)[N], rep) { return ArrayResult{}; }
    };

    inline ArrayResult update(const T (&vals)[N], rep now) const {
        return Runner<0, sizeof...(RulePEs)>::run(rules, vals, now);
    }
    inline ArrayResult update(const std::array<T,N>& vals, rep now) const {
        return update(*reinterpret_cast<const T(*)[N]>(vals.data()), now);
    }

    inline void reset_all() { resetter<0, sizeof...(RulePEs)>::apply(rules); }

private:
    template<std::size_t I, std::size_t M>
    struct resetter {
        static inline void apply(std::tuple<RulePEs*...>& t) {
            auto* r = std::get<I>(t);
            if (r) r->reset_all();
            resetter<I+1, M>::apply(t);
        }
    };
    template<std::size_t M>
    struct resetter<M, M> { static inline void apply(std::tuple<RulePEs*...>&) {} };
};

// Helper factories

template<class Rule, std::size_t N, class InitFn>
inline PerElement<Rule,N> make_per_element(InitFn init) {
    PerElement<Rule,N> pe;
    for (std::size_t i = 0; i < N; ++i) init(pe.r[i], i);
    return pe;
}

template<class Rule, std::size_t N>
inline PerElement<Rule,N> make_per_element_same(const Rule& proto) { return PerElement<Rule,N>(proto); }

template<class T, std::size_t N, class Time, class Reduce, class... RulePEs>
inline EnvelopeArrayT<T,N,Time,Reduce,RulePEs...> make_envelopeArrayT(RulePEs&... rs) {
    return EnvelopeArrayT<T,N,Time,Reduce,RulePEs...>(rs...);
}


} // namespace envelope
