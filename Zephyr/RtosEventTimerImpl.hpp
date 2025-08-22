// Zephyr/RtosEventTimerImpl.hpp
#pragma once

#include <zephyr/kernel.h>
#include <functional>

class RtosEventTimerImpl {
public:
    using Callback = std::function<void()>;

    RtosEventTimerImpl(const char* name, uint32_t periodMs, bool periodic, Callback cb)
        : _callback(cb), _period(K_MSEC(periodMs)), _isPeriodic(periodic) {
        k_timer_init(&_timer, &RtosEventTimerImpl::zephyrHandler, nullptr);
        k_timer_user_data_set(&_timer, this);
    }

    void start() {
        if (_isPeriodic)
            k_timer_start(&_timer, _period, _period);
        else
            k_timer_start(&_timer, _period, K_NO_WAIT);
    }

    void stop() {
        k_timer_stop(&_timer);
    }

    ~RtosEventTimerImpl() {
        k_timer_stop(&_timer);
    }

private:
    struct k_timer _timer;
    k_timeout_t _period;
    bool _isPeriodic;
    Callback _callback;

    static void zephyrHandler(struct k_timer* timer) {
        auto* self = static_cast<RtosEventTimerImpl*>(k_timer_user_data_get(timer));
        if (self && self->_callback) self->_callback();
    }
};
