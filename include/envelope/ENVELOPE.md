# envelope.hpp — Minimal embedded “envelope/limits” library

Header-only C++14 library for validating values (scalars or arrays) against declarative limits with debounced state and fixed, priority-ordered rules.

* **Zero-alloc** (no `new`/`delete`), **no exceptions/RTTI**.
* **Per-rule timers** for enter/exit delays (debounce, linger).
* **Rule kinds:** `Above`, `Below`, `Within`, `Outside` + hysteresis variants.
* **Aggregators:** scalar (`Envelope`, `EnvelopeT`) and array (`EnvelopeArrayT`) with pluggable reducers.
* **Portable time policy:** floating-point seconds or wrap-safe unsigned tick counters.

---
## 1 Envelope System Quick Starts

Below are several small, copy‑pasteable ways to wire up the envelope/limit checker. Each snippet uses `float` values and `SecondsTime` unless noted.

---
### 1.1 Minimal: single rule
```cpp
#include "envelope.hpp"
using Seconds = envelope::SecondsTime;

int main() {
    using namespace envelope;
    auto within = NORMAL_WITHIN<float, Seconds>(0.0f, 10.0f); // ok in [0,10]
    Envelope<float, 1, Seconds> env{};
    env.bind(0, within);

    float v = 7.5f; float now = 0.12f;
    Result r = env.update(v, now);
    if (r.state == State::Violation) { /* handle */ }
}
```

---
### 1.2 Multiple mixed rules (type‑erased `Envelope`)

Bind order = priority (index 0 wins if several would fire the same tick).

```cpp
using namespace envelope;
Envelope<float, 3, Seconds> env{};

auto not_below = NORMAL_ABOVE<float, Seconds>(-1.0f, 0.02f); // 20ms enter delay
auto inside     = NORMAL_WITHIN<float, Seconds>(0.0f, 10.0f);
Above<float, Seconds> ceiling; ceiling.hi = 100.0f;

env.bind(0, not_below);
env.bind(1, inside);
env.bind(2, ceiling);

Result r = env.update(measurement, time_s);
if (r.state == State::Violation) {
    switch (r.index) { /* 0,1,2 -> which rule */ }
}
```

---

### 1.3 Zero‑overhead calls (`EnvelopeT`)

No function pointers; compiler can inline rule calls. You pass pointers to existing rule objects.

```cpp
using namespace envelope;
Below<float, Seconds> low{ .lo = 0.0f };
Within<float, Seconds> band{ .lo = 1.0f, .hi = 3.0f };
Above<float, Seconds> high{ .hi = 5.0f };

// Helper deduces template args
auto env = make_envelopeT<float, Seconds>(low, band, high);

Result r = env.update(meas, now);
```

---

### 1.4 Debounce (enter/exit delays)

Stabilize flapping signals with per‑rule timers.

```cpp
using namespace envelope;
Above<float, Seconds> spikes;
spikes.hi = 2.5f;
spikes.enter_delay = 0.050f; // 50 ms above 2.5 required to trigger
spikes.exit_delay  = 0.030f; // 30 ms back under to clear

Envelope<float, 1, Seconds> env{}; env.bind(0, spikes);
Result r = env.update(v, t);
```

---

### 1.5 Hysteresis bands (inner/outer)

Avoid chatter near thresholds.

```cpp
using namespace envelope;
// Normal inside [lo_exit, hi_exit]; violation outside [lo_enter, hi_enter]
auto hyst = NORMAL_WITHIN_HYST<float, Seconds>(
    /*lo_enter*/ 0.0f, /*lo_exit*/ 0.5f,
    /*hi_exit*/  2.5f, /*hi_enter*/ 3.0f,
    /*enter*/ 0.04f, /*exit*/ 0.02f);

Envelope<float, 1, Seconds> env{}; env.bind(0, hyst);
```

---

### 1.6 Arrays of sensors (per‑element + reducer)

Run the same rule over N channels and reduce with a policy ("any", "K of N", runs, etc.).

```cpp
using namespace envelope;
constexpr std::size_t N = 8;
PerElement<Within<float, Seconds>, N> per;
for (std::size_t i=0;i<N;++i) { per[i].lo = 0.0f; per[i].hi = 10.0f; per[i].enter_delay = 0.02f; }

// Violate if at least 3 channels are outside the band
using EnvArr = EnvelopeArrayT<float, N, Seconds, CountAtLeast<3>, decltype(per)>;
EnvArr env(per);

float vals[N] = {/*...*/}; float now = 1.23f;
ArrayResult ar = env.update(vals, now);
if (ar.state == State::Violation) {
    // ar.rule_index (which rule), ar.first_index (first bad element), ar.count (how many)
}
```

Other reducers available: `AnyElement`, `FractionAtLeast<num,den>`, `RunLengthAtLeast<L>`, `AllElements`.

---

### 1.7 Unsigned tick time policy

Use an integer tick counter (wrap‑safe elapsed math is built in).

```cpp
struct Ticks { using rep = std::uint32_t; };
using namespace envelope;
Below<float, Ticks> ok; ok.lo = 100.0f; ok.enter_delay = 50; ok.exit_delay = 20; // ticks
Envelope<float, 1, Ticks> env{}; env.bind(0, ok);

Ticks::rep t = read_hw_ticks();
Result r = env.update(value, t);
```

---
### 1.8 Reset between sessions
Clear debounce state on rules (and arrays of rules).

```cpp
using namespace envelope;
Within<float, Seconds> w{ .lo=0, .hi=1 };
Envelope<float,1,Seconds> env{}; env.bind(0, w);

// ... run ...
// when a scenario ends:
w.reset(); // or for typed aggregators with many rules: env.reset_all();
```

---
### 1.9 Custom rule (any callable that matches the signature)

You can plug in your own struct/lambda as long as it exposes `bool operator()(T, Time::rep)` and (optionally) `reset()`.

```cpp
struct SpikeWindow : envelope::Debounce<envelope::SecondsTime> {
    float thresh{5.0f};
    bool operator()(float v, float now) const {
        return const_cast<SpikeWindow*>(this)->Debounce::step(v > thresh, now);
    }
};

SpikeWindow sw; sw.thresh = 6.0f; sw.enter_delay = 0.01f;

envelope::Envelope<float,1,envelope::SecondsTime> env{}; env.bind(0, sw);
```

---
### 1.10 Inclusive vs exclusive bounds

All rules accept `Bounds<true>` to make edges inclusive.

```cpp
using namespace envelope;
Within<float, Seconds, Bounds<true>> closed; // [lo, hi] inclusive
closed.lo = 0.0f; closed.hi = 1.0f;
```

---
## 2 Core concepts

### 2.1 Rules

A **rule** evaluates a value and returns a *stable* boolean: `true` means **violation**. Stability is achieved via a built‑in **debounce** (`enter_delay`, `exit_delay`).

Available rule types (for value type `T`, time policy `Time`, boundary policy `B`):

* `Above<T,Time,B>` — violate if `v > hi`.
* `Below<T,Time,B>` — violate if `v < lo`.
* `Within<T,Time,B>` — violate if `v` is **outside** `[lo, hi]`.
* `Outside<T,Time,B>` — violate if `v` is **inside** `[lo, hi]` (forbidden band).
* **Hysteresis** variants:

  * `WithinHysteresis<T,Time,B>`: normal inside inner band `[lo_exit, hi_exit]`; enter violation when leaving a wider outer band `[lo_enter, hi_enter]`; clear once back in inner band.
  * `OutsideHysteresis<T,Time,B>`: violation inside inner band; clear only after leaving a wider outer band.

Each rule inherits `Debounce<Time>` and exposes:

* `enter_delay`, `exit_delay` (time to enter/leave violation);
* `reset()` to return to normal immediately.

### 2.2 Aggregators (scalar)

Combine rules in priority order; the **first rule** that reports violation wins.

* `Envelope<T, N, Time>` — runtime-typed; bind with `bind(i, rule)`.
* `EnvelopeT<T, Time, Rule...>` — **compile-time typed**; constructed via `make_envelopeT<T,Time>(r0, r1, ...)`.

Both return `Result { state, index }` where `index` is the violating rule’s position (or `0xFF` when normal).

### 2.3 Array mode

Apply a rule per element with **independent debounce state** and reduce to a single array decision.

* **Per-element containers**:

  * `PerElement<Rule, N>` (fixed size) and `PerElementDyn<Rule, MaxN>` (runtime length ≤ MaxN).
* **Reducers**: decide when the array violates.

  * `AnyElement`, `AllElements`, `CountAtLeast<K>`, `FractionAtLeast<Num,Den>`, `RunLengthAtLeast<L>`.
* **Aggregator**: `EnvelopeArrayT<T, N, Time, Reduce, RulePE...>`; returns `ArrayResult { state, rule_index, first_index, count }`.

---

## 3 Time policies

A time policy supplies a `using rep = ...` type to represent time. Supported:

* Floating-point seconds (e.g., `float`) via `SecondsTime`.
* Unsigned integer ticks (`uint32_t`, `uint64_t`) — **wrap-safe**: elapsed uses modular subtraction.

Create your own:

```cpp
struct Ticks32 { using rep = std::uint32_t; }; // e.g., SysTick at 1 kHz
```

---

## 4 API reference (selected)

### 4.1 Rules

```cpp
template<class T, class Time = SecondsTime, class B = Bounds<false>> struct Above : Debounce<Time> { T hi; bool operator()(const T&, Time::rep now) const; };
template<class T, class Time = SecondsTime, class B = Bounds<false>> struct Below : Debounce<Time> { T lo; bool operator()(const T&, Time::rep now) const; };
template<class T, class Time = SecondsTime, class B = Bounds<false>> struct Within : Debounce<Time> { T lo, hi; bool operator()(const T&, Time::rep now) const; };
template<class T, class Time = SecondsTime, class B = Bounds<false>> struct Outside : Debounce<Time> { T lo, hi; bool operator()(const T&, Time::rep now) const; };

template<class T, class Time = SecondsTime, class B = Bounds<false>> struct WithinHysteresis : Debounce<Time> {
  T lo_enter, hi_enter, lo_exit, hi_exit; bool operator()(const T&, Time::rep now) const; };

template<class T, class Time = SecondsTime, class B = Bounds<false>> struct OutsideHysteresis : Debounce<Time> {
  T lo_enter, hi_enter, lo_exit, hi_exit; bool operator()(const T&, Time::rep now) const; };
```

Common members (via `Debounce<Time>`):

```cpp
Time::rep enter_delay{0}, exit_delay{0};
void reset();
```

**Presets** (helpers that construct common rules quickly):

```cpp
NORMAL_BELOW<T,Time,B>(th, enter_delay=0, exit_delay=0)   -> Above
NORMAL_ABOVE<T,Time,B>(th, enter_delay=0, exit_delay=0)   -> Below
NORMAL_WITHIN<T,Time,B>(lo, hi, enter_delay=0, exit_delay=0) -> Within
NORMAL_OUTSIDE<T,Time,B>(lo, hi, enter_delay=0, exit_delay=0) -> Outside
NORMAL_WITHIN_HYST<T,Time,B>(lo_enter, lo_exit, hi_exit, hi_enter, enter_delay=0, exit_delay=0)
NORMAL_OUTSIDE_HYST<T,Time,B>(lo_exit, lo_enter, hi_enter, hi_exit, enter_delay=0, exit_delay=0)
```

### 4.2 Aggregators (scalar)

```cpp
template<class T, std::size_t N, class Time=SecondsTime>
struct Envelope { void bind(std::size_t i, const Rule&); Result update(const T& v, Time::rep now) const; };

template<class T, class Time, class... RuleTypes>
struct EnvelopeT { explicit EnvelopeT(RuleTypes&...); Result update(const T& v, Time::rep now) const; void reset_all(); };
// Helper: make_envelopeT<T,Time>(r0, r1, ...)
```

### 4.3 Array support

```cpp
template<class Rule, std::size_t N> struct PerElement { std::array<Rule,N> r; void reset_all(); };
template<class Rule, std::size_t MaxN> struct PerElementDyn { std::array<Rule,MaxN> r; std::size_t n; void set_size(std::size_t); void reset_all(); };

struct AnyElement; template<std::size_t K> struct CountAtLeast; template<std::size_t Num, std::size_t Den> struct FractionAtLeast; template<std::size_t L> struct RunLengthAtLeast; struct AllElements;

template<class T, std::size_t N, class Time, class Reduce, class... RulePEs>
struct EnvelopeArrayT { ArrayResult update(const T (&vals)[N], Time::rep now) const; void reset_all(); };
// Helpers: make_per_element(initFn), make_per_element_same(proto), make_envelopeArrayT(...)
```

`ArrayResult` fields:

* `state`: `Normal` or `Violation`.
* `rule_index`: which rule (priority order) tripped.
* `first_index`: first offending element or start of run.
* `count`: number of offending elements or run length.

---

## 5 Recipes & examples

### 5.1 Scalar: temperature monitor

```cpp
using Seconds = envelope::SecondsTime;
auto within = envelope::NORMAL_WITHIN<float, Seconds>(18.0f, 24.0f, 2.0f, 5.0f); // 2s enter, 5s exit delay
auto spike  = envelope::NORMAL_BELOW<float, Seconds>(40.0f); // >40°C trips immediately

auto env = envelope::make_envelopeT<float, Seconds>(spike, within); // spike has higher priority

for (;;) {
    float t = read_temp(); float now = uptime_s();
    auto r = env.update(t, now);
    if (r.state == envelope::State::Violation) handle_alarm(r.index);
}
```

### 5.2 Ticks-based time (no floats)

```cpp
struct Ticks { using rep = std::uint32_t; }; // e.g., 1 kHz tick
auto lim = envelope::NORMAL_ABOVE<int, Ticks>(-5, 50, 100); // 50 ticks in, 100 ticks out
```

### 5.3 Hysteresis band to avoid chattering

```cpp
using Sec = envelope::SecondsTime;
// Inner band [-1,+1], outer band [-1.5,+1.5]
auto band = envelope::NORMAL_WITHIN_HYST<float, Sec>(-1.5f, -1.0f, +1.0f, +1.5f, 0.1f, 0.3f);
```

### 5.4 Array: PSD “fingerprint” monitoring

Compare a live PSD to a reference; look for **any severe spike** or a **contiguous widened band**.

```cpp
namespace vib {
using Sec = envelope::SecondsTime; constexpr size_t N = 512; using Arr = std::array<float,N>;

inline void psd_delta_db(const Arr& psd, const Arr& ref, Arr& delta, float eps=1e-12f){
  for(size_t i=0;i<N;++i){ float a=psd[i]<eps?eps:psd[i]; float b=ref[i]<eps?eps:ref[i]; delta[i]=10.0f*std::log10(a/b);} }

struct Rules {
  envelope::PerElement<envelope::WithinHysteresis<float,Sec>, N> within; // shape change
  envelope::PerElement<envelope::Above<float,Sec>, N> spikes;            // > spike_db
  explicit Rules(float tol=3, float margin=2, float spike_db=10){
    for(size_t i=0;i<N;++i){ auto& r=within.r[i]; r.lo_exit=-tol; r.hi_exit=+tol; r.lo_enter=-(tol+margin); r.hi_enter=+(tol+margin); r.exit_delay=0.1f; }
    for(size_t i=0;i<N;++i){ auto& s=spikes.r[i]; s.hi=spike_db; }
  }
};

using Any = envelope::AnyElement; using Run8 = envelope::RunLengthAtLeast<8>;
using SpikeEnv = envelope::EnvelopeArrayT<float, N, Sec, Any,  decltype(std::declval<Rules>().spikes)>;
using BandEnv  = envelope::EnvelopeArrayT<float, N, Sec, Run8, decltype(std::declval<Rules>().within)>;

struct Monitor { Rules rules; SpikeEnv e0{rules.spikes}; BandEnv e1{rules.within};
  envelope::ArrayResult update(const Arr& psd, const Arr& ref, float now){ Arr d{}; psd_delta_db(psd,ref,d); auto ar=e0.update(d,now); return (ar.state==envelope::State::Violation)?ar:e1.update(d,now);} };
}
```

### 5.5 Array with “k-of-n” voting

```cpp
constexpr size_t N = 16; using Sec = envelope::SecondsTime;
auto proto = envelope::NORMAL_WITHIN<float, Sec>(-2.0f, 2.0f, 0.05f, 0.10f);
auto per   = envelope::make_per_element_same<envelope::Within<float,Sec>, N>(proto);
using Reduce = envelope::CountAtLeast<4>; // any 4 bins out -> violation
auto env = envelope::make_envelopeArrayT<float, N, Sec, Reduce>(per);
```

---

## 6 Behavioral details

* **Debounce semantics**:

  * When the immediate condition becomes true, the rule waits `enter_delay` before asserting violation.
  * When it becomes false, the rule holds violation for `exit_delay` before clearing.
* **Priority**: Aggregators return the **first** violating rule (index 0 is highest priority).
* **Statefulness**: Rules carry state; don’t share one rule across unrelated signals unless that’s intended. Use `PerElement` for arrays to isolate state.
* **Bounds**: Default is exclusive; switch to `Bounds<true>` for inclusive edges.
* **Time wrap**: If `Time::rep` is an unsigned integer, `elapsed(now, since)` is wrap-safe.

---

## 7 Performance & footprint

* `EnvelopeT` and `EnvelopeArrayT` are fully typed; compilers usually inline rule calls.
* Memory usage is dominated by the number of rules; each rule stores a couple of booleans and timestamps.
* No heap allocations; all containers are `std::array`s with compile-time sizes.

---

## 8 Testing tips

* Drive rules with synthetic waveforms; assert delayed transitions at exact ticks.
* Table-driven tests for inclusivity: values exactly at `lo`/`hi` with both `Bounds<false>` and `Bounds<true>`.
* Array reducers: construct inputs that trigger *just below* and *just above* thresholds (e.g., 3 vs 4 violators for `CountAtLeast<4>`).

---

## 9 Extending the library

* **Custom reducers**: implement a static `eval(pe, values, now, first, count)`.
* **Transforms**: compute derived values (e.g., dB, ratios) before calling `update`. If desired, a future version could add a transform policy to `EnvelopeArrayT`.
* **Masked arrays**: add a reducer that skips indices where `mask[i]==0` or requires weighted fractions.

---

## 10 Gotchas

* Hysteresis thresholds must satisfy ordering:

  * `WithinHysteresis`: `lo_enter ≤ lo_exit ≤ hi_exit ≤ hi_enter`.
  * `OutsideHysteresis`: `lo_exit ≤ lo_enter ≤ hi_enter ≤ hi_exit`.
* Don’t use the **same** rule instance for multiple signals; state will interfere.
* If you need inclusive boundaries, specify `Bounds<true>` explicitly.
* Use `Ticks` time policy on MCUs without floating-point uptime.

---

## 11 License

MIT. See header for SPDX identifier.
