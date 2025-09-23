#pragma once
#include "QMsg.hpp"
#include "RtosMsgBufferTask.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <algorithm>

//--------------------------------------------
/// @brief Abstract base for all topics
///
/// The name_ member must be a string literal and is immutable 
/// after construction.
class TopicBase
{
public:
    explicit TopicBase(const std::string_view n) : name_(n) {}
    virtual ~TopicBase() = default;

    std::string_view getName() const { return name_; }
    [[nodiscard]] virtual size_t notify() = 0;

    /// @brief Add a subscriber to the topic.
    /// @param q Message queue to subscribe.
    /// @param msgId Message ID to subscribe to.
    /// @return True if the subscriber was added, false if it already exists.
    [[nodiscard]] bool addSubscriber(IRtosMsgReceiver &q, uint32_t msgId)
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        auto it = find_if(subscribers_.begin(), subscribers_.end(),
                          [&](const Sub &s)
                          { return s.q == &q && s.id == msgId; });
        if (it == subscribers_.end())
        {
            subscribers_.push_back(Sub{&q, msgId});
            return true;
        }
        return false;
    }

    /// @brief Remove a subscriber from the topic.
    /// @note Must be called before the subscriber is deleted.
    /// @param q Message queue to unsubscribe.
    /// @param msgId Message ID to unsubscribe from.
    /// @return True if the subscriber was removed, false if it did not exist.
    [[nodiscard]] bool removeSubscriber(IRtosMsgReceiver &q, uint32_t msgId)
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        auto it = remove_if(subscribers_.begin(), subscribers_.end(),
                            [&](const Sub &s)
                            { return s.q == &q && s.id == msgId; });
        if (it != subscribers_.end())
        {
            subscribers_.erase(it, subscribers_.end());
            return true;
        }
        return false;
    }

protected:
    struct Sub
    {
        IRtosMsgReceiver *q;
        uint32_t id;
    };
    std::vector<Sub> subscribers_;
    mutable std::mutex subs_mtx_;

private:
    const std::string_view name_;
};

//--------------------------------------------
/// @brief Typed topic: publishes QMsg<PayloadType>
///
/// Access to the topic is exclusive to one thread at a time.
/// The topic maintains a list of subscribers (IRtosMsgReceiver + msgId).
template <typename PayloadType>
class Topic : public TopicBase
{
    using WriteCb = std::function<bool(const PayloadType &)>;

public:
    /// @brief Construct a new Topic
    /// @param name Topic name (must be unique and a literal string)
    /// @param cb Optional write callback
    Topic(const std::string_view name, WriteCb cb = nullptr) : TopicBase(name), writeCallback_(std::move(cb)) {}

    /// @brief Notify all subscribers of a new message. Can only be called
    /// from the thread that owns the topic.
    /// @return Number of errors encountered during notification, one per failed subscriber.
    size_t notify() override
    {
        std::vector<Sub> subs_copy;
        {
            std::lock_guard<std::mutex> lk(subs_mtx_);
            subs_copy = subscribers_; // vector copy
        }
        size_t errors = 0;
        for (auto &s : subs_copy)
        {
            msg.cmd = s.id;
            if (s.q->send(&msg, msg.size()) != msg.size())
                errors++;
        }
        return errors;
    }

    /// @brief Get a reference to the topic data for reading or modification.
    /// @note Can only be called from the thread that owns the topic.
    /// @return Reference to the topic data.
    PayloadType &data() { return msg.getData(); }

    /// @brief Request a write to the topic. Returns false if writes are not
    /// supported or if the write callback returns false.
    /// @param value Payload value
    /// @note This does NOT notify subscribers. The owner of the topic will
    /// need to call notify() after validating and applying the write.
    /// @note Callbacks must be non-throwing.
    bool requestWrite(const PayloadType &value)
    {
        if (!writeCallback_)
            return false; // writes not supported for this topic
        return writeCallback_(value);
    }

private:
    QMsg<uint32_t, PayloadType> msg;
    WriteCb writeCallback_;
};

//--------------------------------------------
/// @brief Central message bus for topics. Allows topic registration,
/// subscription, and message publishing. All methods are thread-safe.
/// It is intended for embedded systems with limited resources.
///
/// Topics are identified by name (string) and must be registered
/// before they can be used and cannot be unregistered.
///
/// Registered topics can never be deleted.
///
/// Subscriptions are per-topic and per-receiver (IRtosMsgReceiver).
class MsgBus
{
public:
    /// @brief Register a topic with the message bus.
    /// @tparam T Payload type
    /// @param topic Pointer to the topic to register. Must not be null.
    /// @return True if registration was successful, false if a topic with
    /// the same name already exists.
    /// @note The topic must remain valid for the lifetime of the MsgBus.
    template <typename T>
    [[nodiscard]] static bool registerTopic(Topic<T> *topic)
    {
        if (!topic)
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto name = std::string(topic->getName());
        auto [it, inserted] = topics_.emplace(std::move(name), topic);
        return inserted;
    }

    /// @brief Subscribe a receiver to a topic.
    /// @param name Topic name
    /// @param receiver Receiver message buffer
    /// @param msgId Message ID
    /// @return True if subscription was successful, false otherwise.
    /// @note Before a subscriber is deleted, it must unsubscribe from all topics.
    /// Otherwise, the MsgBus will hold a dangling pointer.
    [[nodiscard]] static bool subscribe(const char *name, IRtosMsgReceiver &receiver, uint32_t msgId)
    {
        TopicBase *topic = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name);
            if (it == topics_.end())
                return false;
            topic = it->second; // safe: topics are never unregistered
        }
        return topic->addSubscriber(receiver, msgId); // no bus lock held here
    }

    /// @brief Unsubscribe a receiver from a topic.
    /// @param name Topic name
    /// @param receiver Receiver message buffer
    /// @param msgId Message ID
    /// @return True if unsubscription was successful, false otherwise.
    [[nodiscard]] static bool unsubscribe(const char *name, IRtosMsgReceiver &receiver, uint32_t msgId)
    {
        TopicBase *topic = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name);
            if (it == topics_.end())
                return false;
            topic = it->second; // safe: topics are never unregistered
        }
        return topic->removeSubscriber(receiver, msgId); // no bus lock held here
    }

    /// @brief Write a value to a topic. The topic must support writes
    /// (have a write callback).
    /// @tparam T Payload type
    /// @param name Topic name
    /// @param value Payload value
    /// @return True if the write request was successful, false otherwise.
    ///  @note This does NOT notify subscribers. The owner of the topic will
    /// need to call notify() after validating and applying the write.
    template <typename T>
    [[nodiscard]] static bool requestWrite(const char *name, const T &value)
    {
        TopicBase *base = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name);
            if (it == topics_.end())
                return false;
            base = it->second;
        }
        auto *typed = dynamic_cast<Topic<T> *>(base);
        return typed ? typed->requestWrite(value) : false;
    }

private:
    inline static std::map<std::string, TopicBase *> topics_{}; // or unordered_map
    inline static std::mutex mutex_;
};
