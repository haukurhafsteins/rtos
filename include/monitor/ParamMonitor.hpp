#pragma once

#include "MinMaxAvg.hpp"
#include "envelope/envelope.hpp"
#include "MsgBus.hpp"

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
    ParamMonitor(const char *name) : topic_value(name), topic_stats(name), topic_violation(name),
                                     minMaxAvg_(DEFAULT_WINDOW)
    {
        std::string topicValueName = std::string(name) + ".value";
        std::string topicStatsName = std::string(name) + ".stats";
        std::string topicViolationName = std::string(name) + ".violation";
        MsgBus::registerTopic<T>(&topic_value);
        MsgBus::registerTopic<StatsT>(&topic_stats);
        MsgBus::registerTopic<envelope::Result>(&topic_violation);
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
        topic_value.data() = value;
        topic_value.notify();
        if (minMaxAvg_.add(value, now))
        {
            StatsT stats;
            if (minMaxAvg_.getRange(stats))
            {
                topic_stats.data() = stats;
                topic_stats.notify();
            }
            minMaxAvg_.reset();
        }
        envelope::Result envState = env.update(value, now.count() / 1000.0f);
        if (envState.state != lastState_.state)
        {
            topic_violation.data() = envState;
            topic_violation.notify();
            lastState_ = envState;
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
        if (next < MAX_RULES)
            env.bind(next++, r);
    }

    // Get the rule that caused the last violation, or nullptr if none
    template <class Rule>
    const Rule *getViolationRule(envelope::Result &res) const
    {
        if (res.state == envelope::State::Violation && res.index < MAX_RULES)
        {
            return static_cast<const Rule *>(env.rules[res.index]);
        }
        return nullptr;
    }

    const MinMaxAvg<T> &getStats() const { return minMaxAvg_; }
    void resetStats() { minMaxAvg_.reset(); }

private:
    Topic<T> topic_value;
    Topic<StatsT> topic_stats;
    Topic<envelope::Result> topic_violation;

    MinMaxAvgWindowed<T> minMaxAvg_;
    std::size_t next;
    envelope::Result lastState_{envelope::State::Normal, envelope::Result::NO_VIOLATION};

    envelope::Envelope<T, MAX_RULES, Seconds> env{};
};