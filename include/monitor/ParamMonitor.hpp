#pragma once

#include <string>
#include <string_view>
#include "MinMaxAvg.hpp"
#include "MsgBus.hpp"
#include "envelope/envelope.hpp"
#include "monitor/ParamConfig.hpp"

using namespace std;
using Seconds = envelope::SecondsTime;
using Millis = rtos::time::Millis;

/// @brief Monitors a parameter over time, providing statistics and envelope detection.
/// Sends updates via MsgBus topics for value, min/avg/max, and violations.
/// @tparam T The type of the parameter being monitored.
template <typename T>
class ParamMonitor
{
    using StatsT = typename MinMaxAvg<T>::Stats;

public:
    constexpr static size_t MAX_RULES = 4;
    constexpr static Millis DEFAULT_WINDOW = Millis(60000); // 60s

    /// @brief Construct a new Param Monitor
    /// @param name The name of the parameter to monitor, must be
    /// unique and a literal string.
    ///
    /// This creates three topics:
    /// - <name>.value: publishes the latest value (float)
    /// - <name>.stats: publishes MinMaxAvg stats every window
    /// - <name>.violation: publishes envelope violation events
    ParamMonitor(const string_view name, ParamConfig &paramCfg) : _topic_value(name), _topic_stats(name), _topic_violation(name),
                                                                   _minMaxAvg(DEFAULT_WINDOW)
    {
        string topicValueName = string(name) + ".value";
        string topicStatsName = string(name) + ".stats";
        string topicViolationName = string(name) + ".violation";
        MsgBus::registerTopic<T>(&_topic_value);
        MsgBus::registerTopic<StatsT>(&_topic_stats);
        MsgBus::registerTopic<envelope::Result>(&_topic_violation);
        // addEnvelopeRule(paramCfg->getRules(name));
    };
    ~ParamMonitor() = default;

    /// @brief Add a new sample with timestamp.
    /// @param value Sample value
    /// @param now Current time in milliseconds
    /// @return The current envelope state after adding the sample.
    ///
    /// This method publishes the value under <name>.value, adds a new sample
    /// to the MinMaxAvg window and the envelope.
    /// If the MinMaxAvg window has elapsed, it publishes the stats to the
    /// <name>.stats topic and resets the stats.
    /// If the envelope state has changed, it publishes the new state to the
    /// <name>.violation topic.
    [[nodiscard]] envelope::Result update(float value, Millis now)
    {
        _topic_value.data() = value;
        _topic_value.notify();
        if (_minMaxAvg.add(value, now))
        {
            StatsT stats;
            if (_minMaxAvg.getRange(stats))
            {
                _topic_stats.data() = stats;
                _topic_stats.notify();
            }
            _minMaxAvg.reset();
        }
        envelope::Result envState = _env.update(value, now.count() / 1000.0f);
        if (envState.state != _lastState.state)
        {
            _topic_violation.data() = envState;
            _topic_violation.notify();
            _lastState = envState;
        }
        return envState;
    }

    /// @brief Add an envelope rule to the monitor.
    /// @param r The rule to add. The rule must outlive the ParamMonitor.
    ///
    /// Rules are evaluated in the order they are added.
    /// If the maximum number of rules is exceeded, the rule is ignored.
    /// See envelope.hpp for rule types.
    /// Example:
    /// ```
    /// ParamMonitor<float> pm("temperature");
    /// StaticEnvelopeRule<float, Seconds> r1{30.0f, 60.0f}; // 30 degC for 60s
    /// pm.addEnvelopeRule(r1);
    /// ```
    template <class Rule>
    void addEnvelopeRule(const Rule &r)
    {
        if (_next < MAX_RULES)
            _env.bind(_next++, r);
    }

    // Get the rule that caused the last violation, or nullptr if none
    template <class Rule>
    const Rule *getViolationRule(envelope::Result &res) const
    {
        if (res.state == envelope::State::Violation && res.index < MAX_RULES)
        {
            return static_cast<const Rule *>(_env.rules[res.index]);
        }
        return nullptr;
    }

    const MinMaxAvg<T> &getStats() const { return _minMaxAvg; }
    void resetStats() { _minMaxAvg.reset(); }

private:
    Topic<T> _topic_value;
    Topic<StatsT> _topic_stats;
    Topic<envelope::Result> _topic_violation;

    MinMaxAvgWindowed<T> _minMaxAvg;
    size_t _next;
    envelope::Result _lastState{envelope::State::Normal, envelope::Result::NO_VIOLATION};

    envelope::Envelope<T, MAX_RULES, Seconds> _env{};
};