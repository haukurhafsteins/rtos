// MessageTimer.hpp
#pragma once

#include "RtosEventTimer.hpp"
#include "MessageTask.hpp"

// Sends a static message to a MessageTask<T> after a delay or periodically

template<typename T>
class MessageTimer {
public:
    MessageTimer(const char* name,
                 uint32_t periodMs,
                 bool periodic,
                 MessageTask<T>& targetTask,
                 const T& message)
        : _message(message),
          _timer(name, periodMs, periodic, [this, &targetTask]() {
              targetTask.send(_message);
          }) {}

    void start() { _timer.start(); }
    void stop()  { _timer.stop();  }

private:
    T _message;
    RtosEventTimer _timer;
};
// Usage example:
// MessageTimer<MyMessageType> timer("MyTimer", 1000, true, myMessageTask, myMessage);
// timer.start();
// ...
// timer.stop();
// This will send myMessage to myMessageTask every 1000 ms (1 second) until stopped.