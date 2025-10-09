#pragma once

#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"
#include "rtosutils.h"
#include "time.hpp"

using rtos::time::Millis;
using rtos::time::now_ms;

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

    void start() { _task.start(); }
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
    static void TaskEntry(void* p) { static_cast<RtosMsgBufferTask*>(p)->taskLoop(); }

    void taskLoop()
    {
        Millis timeoutTimeMs = _receiveTimeoutMs;
        taskEntry();
        while (true)
        {
            if (_receiveTimeoutMs == RTOS_TASK_WAIT_FOREVER)
            {
                size_t len = _msgQueue.receive(_msg, sizeof(_msg), _receiveTimeoutMs);
                std::span<const std::byte> data{reinterpret_cast<const std::byte*>(_msg), len};
                handleMessage(data);
            }
            else
            {
                Millis startTimeMs = now_ms();
                size_t len = _msgQueue.receive(_msg, sizeof(_msg), _receiveTimeoutMs);
                std::span<const std::byte> data{reinterpret_cast<const std::byte*>(_msg), len};
                if (len > 0)
                {
                    handleMessage(data);
                }
                else
                    handleTimeout();
                Millis endTimeMs = now_ms();
                timeoutTimeMs = _receiveTimeoutMs - (endTimeMs - startTimeMs);
                if (timeoutTimeMs < Millis(0))
                {
                    handleTimeoutError();
                    timeoutTimeMs = _receiveTimeoutMs;
                }
            }
        }
    }

    RtosMsgBuffer _msgQueue;
    uint8_t _msg[MaxMsgSize]{};
    RtosTask _task;
    Millis _receiveTimeoutMs;
    Millis _sendTimeoutMs;
};
