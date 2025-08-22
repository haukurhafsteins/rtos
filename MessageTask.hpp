// MessageTask.hpp
#pragma once

#include "RtosQueue.hpp"
#include "RtosTask.hpp"

// MessageTask wraps a task with a message queue

template<typename T>
class MessageTask {
public:
    MessageTask(const char* name, size_t stackSize, int priority, size_t queueLength)
        : _queue(queueLength), _name(name), _stackSize(stackSize), _priority(priority) {
#if defined(USE_FREERTOS)
        _task = new RtosTask(name, stackSize, priority, &taskEntryPoint, this);
#elif defined(USE_ZEPHYR)
        _task = new RtosTask(name, stackSize, priority, &taskEntryPoint, this, nullptr, nullptr);
#endif
    }

    virtual ~MessageTask() {
        delete _task;
    }

    bool send(const T& msg) {
        return _queue.send(msg);
    }

protected:
    virtual void handleMessage(const T& msg) = 0;

private:
    static
#if defined(USE_FREERTOS)
    void
#elif defined(USE_ZEPHYR)
    void
#endif
    taskEntryPoint(void* p1
#if defined(USE_ZEPHYR)
    , void* /*p2*/, void* /*p3*/
#endif
    ) {
        static_cast<MessageTask*>(p1)->taskLoop();
    }

    void taskLoop() {
        T msg;
        while (_queue.receive(msg)) {
            handleMessage(msg);
        }
    }

    RtosQueue<T> _queue;
    RtosTask* _task;
    const char* _name;
    size_t _stackSize;
    int _priority;
};
