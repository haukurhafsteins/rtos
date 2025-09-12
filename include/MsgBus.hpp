#pragma once
#include "QMsg.hpp"
#include "RtosMsgBuffer.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <string>

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
        std::lock_guard<std::mutex> lk(subs_mtx_);
        subscribers_.push_back({&q, msgId});
    }
    void removeSubscriber(RtosMsgBuffer &q)
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
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
    mutable std::mutex subs_mtx_;

private:
    const char *name_;
};

//--------------------------------------------
// Typed topic: publishes QMsg<PayloadType>
template <typename PayloadType>
class Topic : public TopicBase
{
public:
    Topic(const char *name) : TopicBase(name) {}

    bool notify() override
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        for (auto &s : this->subscribers_)
        {
            msg.cmd = s.id;
            s.q->send(&msg, msg.size());
        }
        return true;
    }

    PayloadType &data() { return msg.getData(); }
    bool publish(const PayloadType &v)
    {
        msg.getData() = v;
        return notify();
    }

    void setWriteHandler(std::function<bool(const PayloadType &)> cb)
    {
        writeCallback_ = std::move(cb);
    }

    /// @brief Request a write to the topic. Returns false if writes are not
    /// supported or if the write callback returns false.
    /// @note This does NOT notify subscribers. The write callback is responsible
    /// for calling notify() if needed.
    bool requestWriteOnly(const PayloadType &value)
    {
        if (!writeCallback_)
            return false; // writes not supported for this topic
        return writeCallback_(value);
    }

private:
    QMsg<uint32_t, PayloadType> msg;
    std::function<bool(const PayloadType &)> writeCallback_;
};

//--------------------------------------------
/// @brief Central message bus for topics. Allows topic registration,
/// subscription, and message publishing. All methods are thread-safe.
///
/// Topics are identified by name (string) and must be registered
/// before they can be used and cannot be unregistered.
///
/// Subscriptions are per-topic and per-receiver (RtosMsgBuffer).
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
        TopicBase *base = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(std::string(name));
            if (it == topics_.end())
                return false;
            base = it->second; // copy out under lock
        }
        auto *typed = dynamic_cast<Topic<T> *>(base);
        return (!typed) ? false : typed->requestWriteOnly(value);
    }

private:
    inline static std::map<std::string, TopicBase *> topics_{};
    inline static std::mutex mutex_;
};
