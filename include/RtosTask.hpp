#pragma once
#include <atomic>
#include <cstdint>
#include "backend.hpp"

constexpr uint32_t RTOS_TASK_WAIT_FOREVER = 0xffffffffUL;

using TaskFunction = void (*)(void *);

class RtosTask
{
public:
    RtosTask(const char *name, uint32_t stackSize, uint32_t priority, TaskFunction func, void *arg) 
    : name_(name), stackSize_(stackSize), priority_(priority), func_(func), arg_(arg) {}
    ~RtosTask();
    bool start();
    backend::ThreadHandle *handle() const { return handle_; }

protected:
    virtual void taskMain() = 0;
private:
    std::atomic<bool> started_{false};
    rtos::backend::ThreadHandle *handle_{};
    uint32_t stackSize_{};
    uint32_t priority_{};
    TaskFunction func_{};
    void *arg_{};
    const char* name_{};
};