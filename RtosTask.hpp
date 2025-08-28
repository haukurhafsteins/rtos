#pragma once
#include "backend.hpp"
#include <atomic>

constexpr uint32_t RTOS_TASK_WAIT_FOREVER = 0xffffffffUL;

using TaskFunction = void (*)(void *);

class RtosTask
{
public:
    bool start(); // idempotent
    bool isStarted() const;

protected:
    virtual void taskMain() = 0;
    void setStartGate(bool use_gate); // optional
private:
    void static threadTrampoline(void *arg); // calls taskMain()
    backend::ThreadHandle *handle_{};
    std::atomic<bool> started_{false};
    bool use_gate_{true};
};