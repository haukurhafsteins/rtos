#pragma once

#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"
#include "rtosutils.h"

constexpr uint32_t RTOS_TASK_WAIT_FOREVER = 0xffffffffUL;

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
    void receiveTimeout(uint32_t timeoutMs) { _receiveTimeoutMs = timeoutMs; }
    void sendTimeout(uint32_t timeoutMs) { _sendTimeoutMs = timeoutMs; }

protected:
    bool send(const void *data, size_t len) { return _msgQueue.send(data, len, _sendTimeoutMs); }
    virtual void handleMessage(const void *data, size_t len) = 0;
    virtual void handleTimeout() {}
    virtual void handleTimeoutError() {}

private:
    static void TaskEntry(void* p) { static_cast<RtosMsgBufferTask*>(p)->taskLoop(); }

    void taskLoop()
    {
        int32_t timeoutTimeMs = _receiveTimeoutMs;
        while (true)
        {
            if (_receiveTimeoutMs == RTOS_TASK_WAIT_FOREVER)
            {
                size_t len = _msgQueue.receive(_msg, sizeof(_msg), _receiveTimeoutMs);
                handleMessage(_msg, len);
            }
            else
            {
                uint64_t startTimeMs = get_time_in_milliseconds();
                size_t len = _msgQueue.receive(_msg, sizeof(_msg), _receiveTimeoutMs);
                if (len > 0)
                    handleMessage(_msg, len);
                else
                    handleTimeout();
                uint64_t endTimeMs = get_time_in_milliseconds();
                timeoutTimeMs = _receiveTimeoutMs - (endTimeMs - startTimeMs);
                if (timeoutTimeMs < 0)
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
    uint32_t _receiveTimeoutMs;
    uint32_t _sendTimeoutMs;
};
