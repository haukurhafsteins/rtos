#pragma once
#include <string>
#include <cstdio>
#include "envelope/envelope.hpp"

namespace envelope {

// A generic kind that covers all built-in rule types
enum class RuleKind : uint8_t {
    Unknown = 0,
    Below,
    Above,
    Within,
    Outside,
    WithinHysteresis,
    OutsideHysteresis
};

// A plain view of a bound rule (no RTTI, no allocations)
template<class T>
struct RuleView {
    RuleKind kind{RuleKind::Unknown};
    uint8_t  index{Result::NO_VIOLATION};
    T lo{}, hi{};
    T lo_enter{}, hi_enter{}, lo_exit{}, hi_exit{};
    float enter_delay{0}, exit_delay{0};
};

// Inspect the rule stored at env[index] and return a filled view
template<class T, std::size_t N, class Time, class B = Bounds<false>>
inline RuleView<T> inspect_rule(const Envelope<T,N,Time>& env, uint8_t idx) {
    RuleView<T> v{};
    v.index = idx;
    if (idx >= N || !env.vtbl[idx] || !env.rules[idx]) return v;

    using Sec = Time;
    auto* fn = env.vtbl[idx];
    const void* p = env.rules[idx];

    auto is = [&](auto tag){
        using R = decltype(tag);
        return fn == &call_rule<R, T, Sec>;
    };

    if (is(Below<T,Sec,B>{})) {
        auto* r = static_cast<const Below<T,Sec,B>*>(p);
        v.kind=RuleKind::Below; v.lo=r->lo; v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    if (is(Above<T,Sec,B>{})) {
        auto* r = static_cast<const Above<T,Sec,B>*>(p);
        v.kind=RuleKind::Above; v.hi=r->hi; v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    if (is(Within<T,Sec,B>{})) {
        auto* r = static_cast<const Within<T,Sec,B>*>(p);
        v.kind=RuleKind::Within; v.lo=r->lo; v.hi=r->hi; v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    if (is(Outside<T,Sec,B>{})) {
        auto* r = static_cast<const Outside<T,Sec,B>*>(p);
        v.kind=RuleKind::Outside; v.lo=r->lo; v.hi=r->hi; v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    if (is(WithinHysteresis<T,Sec,B>{})) {
        auto* r = static_cast<const WithinHysteresis<T,Sec,B>*>(p);
        v.kind=RuleKind::WithinHysteresis;
        v.lo_enter=r->lo_enter; v.hi_enter=r->hi_enter; v.lo_exit=r->lo_exit; v.hi_exit=r->hi_exit;
        v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    if (is(OutsideHysteresis<T,Sec,B>{})) {
        auto* r = static_cast<const OutsideHysteresis<T,Sec,B>*>(p);
        v.kind=RuleKind::OutsideHysteresis;
        v.lo_enter=r->lo_enter; v.hi_enter=r->hi_enter; v.lo_exit=r->lo_exit; v.hi_exit=r->hi_exit;
        v.enter_delay=r->enter_delay; v.exit_delay=r->exit_delay; return v;
    }
    return v;
}

// ---------- Optional formatting helpers ----------

inline const char* kindToString(RuleKind k) {
    switch (k) {
        case RuleKind::Below: return "Below";
        case RuleKind::Above: return "Above";
        case RuleKind::Within: return "Within";
        case RuleKind::Outside: return "Outside";
        case RuleKind::WithinHysteresis: return "WithinHysteresis";
        case RuleKind::OutsideHysteresis: return "OutsideHysteresis";
        default: return "Unknown";
    }
}

template<class T>
inline std::string toString(const RuleView<T>& v) {
    char b[200];
    switch (v.kind) {
    case RuleKind::Below:  std::snprintf(b,sizeof(b), "Below %.3f (ent %.2fs, ext %.2fs)", (double)v.lo, v.enter_delay, v.exit_delay); break;
    case RuleKind::Above:  std::snprintf(b,sizeof(b), "Above %.3f (ent %.2fs, ext %.2fs)", (double)v.hi, v.enter_delay, v.exit_delay); break;
    case RuleKind::Within: std::snprintf(b,sizeof(b), "Within [%.3f, %.3f] (ent %.2fs, ext %.2fs)", (double)v.lo,(double)v.hi, v.enter_delay, v.exit_delay); break;
    case RuleKind::Outside:std::snprintf(b,sizeof(b), "Outside [%.3f, %.3f] (ent %.2fs, ext %.2fs)", (double)v.lo,(double)v.hi, v.enter_delay, v.exit_delay); break;
    case RuleKind::WithinHysteresis:
        std::snprintf(b,sizeof(b),
            "Outside Hyst: enter [%.3f, %.3f], exit [%.3f, %.3f] (ent %.2fs, ext %.2fs)",
            (double)v.lo_enter,(double)v.hi_enter,(double)v.lo_exit,(double)v.hi_exit, v.enter_delay, v.exit_delay);
        break;
    case RuleKind::OutsideHysteresis:
        std::snprintf(b,sizeof(b),
            "Inside Hyst: enter [%.3f, %.3f], exit [%.3f, %.3f] (ent %.2fs, ext %.2fs)",
            (double)v.lo_enter,(double)v.hi_enter,(double)v.lo_exit,(double)v.hi_exit, v.enter_delay, v.exit_delay);
        break;
    default: std::snprintf(b,sizeof(b), "Unknown rule (index %u)", v.index); break;
    }
    return std::string(b);
}

template<class T>
inline std::string toJson(const RuleView<T>& v, bool pretty=false) {
    auto fmtf = [](double d){ char b[32]; std::snprintf(b,sizeof(b), "%.6g", d); return std::string(b); };
    auto nl = pretty ? "\n" : "";
    auto i1 = pretty ? "  " : "";
    auto sp = pretty ? " " : "";
    std::string s; s.reserve(256);
    s += "{"; s += nl;
    s += i1; s += "\"kind\":"; s += sp; s += "\""; s += kindToString(v.kind); s += "\",";
    s += nl; s += i1; s += "\"rule_index\":"; s += sp; s += std::to_string((unsigned)v.index);
    auto add=[&](const char* k, double val){ s += ","; s += nl; s += i1; s += "\""; s += k; s += "\":"; s += sp; s += fmtf(val); };
    switch (v.kind) {
        case RuleKind::Below:  add("lo", v.lo); break;
        case RuleKind::Above:  add("hi", v.hi); break;
        case RuleKind::Within: add("lo", v.lo); add("hi", v.hi); break;
        case RuleKind::Outside:add("lo", v.lo); add("hi", v.hi); break;
        case RuleKind::WithinHysteresis:
        case RuleKind::OutsideHysteresis:
            add("lo_enter", v.lo_enter); add("hi_enter", v.hi_enter);
            add("lo_exit", v.lo_exit);   add("hi_exit",  v.hi_exit);
            break;
        default: break;
    }
    if (v.enter_delay!=0) add("enter_delay", v.enter_delay);
    if (v.exit_delay!=0)  add("exit_delay",  v.exit_delay);
    s += nl; s += "}";
    return s;
}

} // namespace envelope
