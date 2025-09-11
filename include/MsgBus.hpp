#pragma once
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <mutex>
#include <limits>
#include <functional>   // <-- add
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
// Typed topic: publishes QMsg<PayloadType>
template <typename PayloadType>
class Topic : public TopicBase
{
public:
    Topic(const char *name)
        : TopicBase(name) {}

    bool notify() override
    {
        for (auto &s : this->subscribers_)
        {
            msg.cmd = s.id;
            s.q->send(&msg, msg.size());
        }
        return true;
    }

    void setWriteHandler(uint32_t writeCmd,
                         std::function<bool(const PayloadType &)> cb)
    {
        writeCmd_ = writeCmd;
        writeCallback_ = std::move(cb);
    }

    bool requestWrite(const PayloadType &value)
    {
        if (!writeCallback_)
            return false; // writes not supported for this topic
        return writeCallback_(value);
    }
    QMsg<uint32_t, PayloadType> msg;

private:
    uint32_t writeCmd_{std::numeric_limits<uint32_t>::max()};
    std::function<bool(const PayloadType &)> writeCallback_;
};

//--------------------------------------------
// Registry: central message bus
class MsgBus
{
public:
    template <typename T>
    static bool registerTopic(Topic<T> *topic)
    {
        std::lock_guard<std::mutex> lock(mutex_); // <-- add lock for symmetry
        auto strName = std::string(topic->getName());
        auto it = topics_.find(strName);
        if (it != topics_.end())
            return false;
        topics_[strName] = topic;
        return true;
    }
    // Removes the topic from the registry but does NOT delete the topic object.
    // The caller is responsible for managing the lifetime of the topic object.
    // TODO: Anyone can remove any topic. Maybe restrict this to the owner or
    // use a registration token returned by registerTopic?
    static bool remove(const char *name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(std::string(name));
        if (it == topics_.end())
            return false;
        topics_.erase(it);
        return true;
    }

    static bool subscribe(const char *name, RtosMsgBuffer &receiver, uint32_t msgId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(std::string(name));
        if (it == topics_.end())
            return false;
        it->second->addSubscriber(receiver, msgId);
        return true;
    }

    template <typename T>
    static bool write(const char *name, const T &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(std::string(name));
        if (it == topics_.end())
            return false;

        // dynamic_cast is safe since Topic<T> derives from TopicBase (polymorphic)
        auto *typed = dynamic_cast<Topic<T> *>(it->second);
        if (!typed)
            return false; // type mismatch
        return typed->requestWrite(value);
    }

private:
    inline static std::map<std::string, TopicBase *> topics_{};
    inline static std::mutex mutex_;
};
