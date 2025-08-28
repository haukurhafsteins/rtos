// RtosMsgBufferTask.hpp
#pragma once

#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"
#include "rtosutils.h"

// RtosMsgBufferTask wraps a task with a message buffer for variable-sized messages

constexpr uint32_t MSG_TASK_WAIT_FOREVER = 0xffffffffUL;

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
    RtosMsgBufferTask(const char *name, size_t stackSize, int priority, size_t bufferSize)
        : _msgQueue(bufferSize), _name(name), _stackSize(stackSize), _priority(priority)
    {
        _receiveTimeoutMs = RTOS_TASK_WAIT_FOREVER;
        _sendTimeoutMs = RTOS_TASK_WAIT_FOREVER;
    }

    virtual ~RtosMsgBufferTask()
    {
        delete _task;
    }

    void start()
    {
        _task = new RtosTask(_name, _stackSize, _priority, &taskEntryPoint, this);
    }

    void receiveTimeout(uint32_t timeoutMs) { _receiveTimeoutMs = timeoutMs; }
    void sendTimeout(uint32_t timeoutMs) { _sendTimeoutMs = timeoutMs; }

protected:
    bool send(const void *data, size_t len) { return _msgQueue.send(data, len, _sendTimeoutMs); }
    virtual void handleMessage(const void *data, size_t len) = 0;
    virtual void handleTimeout() {}
    virtual void handleTimeoutError() {}

private:
    static void taskEntryPoint(void *p1)
    {
        static_cast<RtosMsgBufferTask *>(p1)->taskLoop();
    }

    void taskLoop()
    {
        int32_t timeoutTimeMs = _receiveTimeoutMs;
        while (true)
        {
            if (_receiveTimeoutMs == MSG_TASK_WAIT_FOREVER)
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

    /// @brief The main buffer thant can store
    RtosMsgBuffer _msgQueue;
    uint8_t _msg[MaxMsgSize];
    const char *_name;
    size_t _stackSize;
    int _priority;
    RtosTask *_task;
    uint32_t _receiveTimeoutMs;
    uint32_t _sendTimeoutMs;
};
