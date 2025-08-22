// RtosMsgBufferTask.hpp
#pragma once

#include "RtosMsgBuffer.hpp"
#include "RtosTask.hpp"

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
        _task = new RtosTask(name, stackSize, priority, &taskEntryPoint, this);
    }

    virtual ~RtosMsgBufferTask()
    {
        delete _task;
    }

    void receiveTimeout(uint32_t timeoutMs) { _receiveTimeoutMs = timeoutMs; }
    void sendTimeout(uint32_t timeoutMs) { _sendTimeoutMs = timeoutMs; }

protected:
    bool send(const void *data, size_t len) { return  _msgQueue.send(data, len, _sendTimeoutMs); }
    virtual void handleMessage(const void *data, size_t len) = 0;
    virtual void handleTimeout() {}

private:
    static void taskEntryPoint(void *p1)
    {
        static_cast<RtosMsgBufferTask *>(p1)->taskLoop();
    }

    void taskLoop()
    {
        while (true)
        {
            size_t len = _msgQueue.receive(_msg, sizeof(_msg), _receiveTimeoutMs);
            if (len > 0)
                handleMessage(_msg, len);
            else
                handleTimeout();
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
