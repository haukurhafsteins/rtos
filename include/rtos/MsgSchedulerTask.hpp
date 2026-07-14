#pragma once

#include "rtos/MsgBufferTask.hpp"
#include "rtos/Singleton.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <list>
#include <algorithm>
#include "rtos/QMsg.hpp"
#include "rtos/time.hpp"

namespace rtos
{

enum class SchedulerCmd : uint8_t
{
    add,
    cancel,
};

struct SMsg; // Forward declaration of SMsg

class MsgSchedulerTask : public MsgBufferTask<sizeof(QMsg<SchedulerCmd, SMsg>*)>, public Singleton<MsgSchedulerTask>
{
public:
    struct SMsg
    {
        IMsgReceiver *task;
        std::chrono::milliseconds period;
        uint64_t nextTime;
        bool periodic;
        uint32_t size;
    };

    MsgSchedulerTask(const char *name, size_t stackSize, int priority, size_t qByteSize)
        : MsgBufferTask(name, stackSize, priority, qByteSize) {}

    /// @brief Schedule a message to be sent to a task after a delay.
    /// @param task The task to send the message to.
    /// @param data Pointer to the message data.
    /// @param size Size of the message data.
    /// @param delay Delay before sending the message.
    /// @param periodic If true, the message will be sent periodically.
    /// @note The message data must be less than the queue size given in the constructor.
    const SMsg* schedule(IMsgReceiver *task,
                  const void *data,
                  size_t size,
                  std::chrono::milliseconds delay,
                  bool periodic = false)
    {
        if (!data || size == 0)
        {
            printf("MsgScheduler: Invalid message buffer or size: %p, %d", data, (int)size);
            return nullptr;
        }

        SMsg *smsg = new SMsg();
        smsg->task = task;
        smsg->period = delay;
        smsg->nextTime = now() + delay.count();
        smsg->periodic = periodic;
        smsg->size = size;
        QMsg<SchedulerCmd, SMsg*> msg(SchedulerCmd::add, smsg);

        if (!send(&msg, msg.size()))
        {
            printf("MsgSchedulerTask::schedule: failed to send message\n");
            delete smsg;
            return nullptr;
        }
        return smsg;
    }

    bool cancel(const SMsg* handle) {
        QMsg<SchedulerCmd, SMsg*> msg(SchedulerCmd::cancel, const_cast<SMsg*>(handle));

        if (!send(&msg, msg.size()))
        {
            printf("MsgSchedulerTask::cancel: failed to send message\n");
            return false;
        }
        return true;
    }

protected:
    void handleMessage(std::span<const std::byte> data) override
    {
        QMsg<SchedulerCmd, SMsg*> *msg = (QMsg<SchedulerCmd, SMsg*> *)data.data();
        switch (msg->cmd)
        {
            case SchedulerCmd::add:
            {
                _list.push_back(msg->data);
                break;
            }
            case SchedulerCmd::cancel:
            {
                _list.remove(msg->data);
                break;
            }
            default:
                printf("MsgScheduler: Invalid command: %d", (int)msg->cmd);
                return;
        }
        processQueue();
        _list.sort([](const SMsg* a, const SMsg* b)
        { return a->nextTime < b->nextTime; });
}

    void handleTimeout() override
    {
        processQueue();
    }

private:
    std::list<SMsg*> _list;

    uint64_t now() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void processQueue()
    {
        uint64_t current = now();
        for (auto it = _list.begin(); it != _list.end() && (*it)->nextTime <= current;)
        {
            (*it)->task->send(static_cast<void*>(&(*it)), (*it)->size);

            if ((*it)->periodic)
            {
                (*it)->nextTime = current + (*it)->period.count();
                it++;
            }
            else
                it = _list.erase(it);
        }

        // Update the timeout to the next event
        if (!_list.empty())
        {
            uint64_t delay = (_list.front()->nextTime > current) ?
                (_list.front()->nextTime - current) : 0;
            receiveTimeout(Millis(delay));
        }
        else
        {
            receiveTimeout(backend::WAIT_FOREVER); // no messages => wait forever
        }
    }

    void printQueue()
    {
        printf("MsgSchedulerTask::printQueue: current queue size: %zu\n", _list.size());
        for (const auto &entry : _list)
        {
            printf("  Task: %p, Period: %lld ms, Next Time: %lld ms\n",
                   entry->task,
                   entry->period.count(),
                   entry->nextTime);
        }
    }
};

} // namespace rtos
