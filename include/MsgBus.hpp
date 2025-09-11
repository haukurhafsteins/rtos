#pragma once
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include "RtosMsgBuffer.hpp"
#include "QMsg.hpp"

//--------------------------------------------
// Abstract base for all topics
class TopicBase
{
public:
    explicit TopicBase(const char *n) : name_(n) {}
    virtual ~TopicBase() = default;

    const char *getName() const { return name_; }
    virtual bool notify() = 0;

    void addSubscriber(RtosMsgBuffer &q, uint32_t msgId)
    {
        subscribers_.push_back({&q, msgId});
    }
    void removeSubscriber(RtosMsgBuffer &q)
    {
        subscribers_.remove_if([q_ptr = &q](const Sub &s)
                               { return s.q == q_ptr; });
    }

protected:
    struct Sub
    {
        RtosMsgBuffer *q;
        uint32_t id;
    };
    std::list<Sub> subscribers_;

private:
    const char *name_;
};

//--------------------------------------------
// Typed topic: publishes QMsg<Cmd, T>
template <typename PayloadType>
class Topic : public TopicBase
{
public:
    Topic(const char *name, const PayloadType *dataPtr)
        : TopicBase(name), data_(dataPtr) {}

    const PayloadType *getData() const { return data_; }

    bool notify() override
    {
        if (!data_)
            return false;
        for (auto &s : this->subscribers_)
        {
            QMsg<uint32_t, PayloadType> msg(static_cast<uint32_t>(s.id), *data_);
            s.q->send(&msg, msg.size());
        }
        return true;
    }

private:
    const PayloadType *data_; // external storage
};

//--------------------------------------------
// Registry: central message bus
class MsgBus
{
public:
    template <typename T>
    static bool registerTopic(const char *name, Topic<T> *topic)
    {
        auto it = topics_.find(name);
        if (it != topics_.end())
            return false;
        topics_[name] = topic;
        return true;
    }

    static bool remove(const char *name)
    {
        auto it = topics_.find(name);
        if (it == topics_.end())
            return false;
        topics_.erase(it);
        return true;
    }

    static bool subscribe(const char *name, RtosMsgBuffer &receiver, uint32_t msgId)
    {
        auto it = topics_.find(name);
        if (it == topics_.end())
            return false;
        it->second->addSubscriber(receiver, msgId);
        return true;
    }

    static TopicBase *get(const char *name)
    {
        auto it = topics_.find(name);
        return (it == topics_.end()) ? nullptr : it->second;
    }

private:
    inline static std::map<std::string, TopicBase *> topics_{};
};
