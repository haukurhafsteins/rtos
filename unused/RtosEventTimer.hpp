// RtosEventTimer.hpp
#pragma once

#include <cstdint>
#include <functional>

class RtosEventTimerImpl;

class RtosEventTimer {
public:
    using Callback = std::function<void()>;

    RtosEventTimer(const char* name, uint32_t periodMs, bool periodic, Callback cb);
    ~RtosEventTimer();

    void start();
    void stop();

private:
    RtosEventTimerImpl* _impl;
};
