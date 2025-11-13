#pragma once

#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"
#include "time.hpp"

using namespace rtos::time;

constexpr Millis RTOS_TASK_WAIT_FOREVER = Millis::max();

class IRtosMsgReceiver
{
public:
    virtual bool send(const void *data, size_t len) = 0;
    virtual ~IRtosMsgReceiver() = default;
};

template <size_t MaxMsgSize>
class RtosMsgBufferTask : public IRtosMsgReceiver
{
public:
    RtosMsgBufferTask(const char *name, uint32_t stack_bytes, uint32_t prio, std::size_t buf_cap)
        : _msgQueue(buf_cap),
          _task(name, stack_bytes, prio, &RtosMsgBufferTask::TaskEntry, this)
    {
        _receiveTimeoutMs = RTOS_TASK_WAIT_FOREVER;
        _sendTimeoutMs = RTOS_TASK_WAIT_FOREVER;
    }

    void start(int core_id = RtosTask::TASK_NO_AFFINITY) { _task.start(core_id); }
    void receiveTimeout(Millis timeoutMs) { _receiveTimeoutMs = timeoutMs; }
    void sendTimeout(Millis timeoutMs) { _sendTimeoutMs = timeoutMs; }
    IRtosMsgReceiver &getMsgReceiver() { return *this; }

protected:
    bool send(const void *data, size_t len)
    {
        if (len > MaxMsgSize)
        {
            printf("ERROR: RtosMsgBufferTask::send: message size %zu exceeds max %zu\n", len, MaxMsgSize);
            return false;
        }
        return _msgQueue.send(data, len, _sendTimeoutMs);
    }
    virtual void taskEntry() {}
    virtual void handleMessage(std::span<const std::byte> data) = 0;
    virtual void handleTimeout() {}
    virtual void handleTimeoutError() {}

private:
    static void TaskEntry(void *p) { static_cast<RtosMsgBufferTask *>(p)->taskLoop(); }

    void taskLoop()
    {
        using Clock = HighResClock;

        auto to_millis = [](auto d)
        { return std::chrono::duration_cast<Millis>(d); };

        taskEntry();

        Millis period = _receiveTimeoutMs;
        bool has_deadline = (period != RTOS_TASK_WAIT_FOREVER);
        Clock::time_point next_deadline = has_deadline ? (Clock::now() + std::chrono::duration_cast<Clock::duration>(period))
                                                       : Clock::time_point{};

        while (true)
        {
            // Compute wait for receive()
            Millis wait_ms = RTOS_TASK_WAIT_FOREVER;
            if (has_deadline)
            {
                auto now = Clock::now();
                auto wait = std::chrono::duration_cast<Millis>(next_deadline - now);
                if (wait < Millis::zero())
                    wait = Millis::zero(); // already late; don't block
                wait_ms = wait;
            }

            size_t len = _msgQueue.receive(_msg, sizeof(_msg), wait_ms);
            std::span<const std::byte> data{reinterpret_cast<const std::byte *>(_msg), len};

            if (len > 0)
            {
                // Message arrived before (or at) the deadline
                handleMessage(data);

                // The handler may change the timeout policy; re-read and adjust.
                Millis new_period = _receiveTimeoutMs;
                if (new_period == RTOS_TASK_WAIT_FOREVER)
                {
                    has_deadline = false; // switch to blocking mode
                }
                else if (!has_deadline || new_period != period)
                {
                    has_deadline = true;
                    period = new_period;
                    next_deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(period); // restart cadence from now
                }
                // If unchanged, keep the existing next_deadline
                continue;
            }

            // receive() timed out (only when has_deadline == true)
            if (has_deadline)
            {
                handleTimeout();

                auto after = Clock::now();
                Millis new_period = _receiveTimeoutMs;

                if (new_period == RTOS_TASK_WAIT_FOREVER)
                {
                    has_deadline = false; // switch to blocking
                    continue;
                }

                if (new_period != period)
                {
                    // Period changed during handler: restart cadence from 'after'
                    period = new_period;
                    next_deadline = after + std::chrono::duration_cast<Clock::duration>(period);
                    continue;
                }

                // Same period: advance schedule and detect overruns (no drift)
                if (after > next_deadline)
                {
                    handleTimeoutError();

                    // Compute how far behind we are and jump forward by whole periods
                    Millis behind_ms = to_millis(after - next_deadline);
                    // Number of whole periods missed (>=1)
                    auto k = static_cast<int64_t>(behind_ms.count() / period.count()) + 1;
                    next_deadline += std::chrono::duration_cast<Clock::duration>(period * k);
                }
                else
                {
                    next_deadline += std::chrono::duration_cast<Clock::duration>(period);
                }
            }
            // In blocking mode, timeouts don’t occur.
        }
    }

    RtosMsgBuffer _msgQueue;
    uint8_t _msg[MaxMsgSize]{};
    RtosTask _task;
    Millis _receiveTimeoutMs;
    Millis _sendTimeoutMs;
};
