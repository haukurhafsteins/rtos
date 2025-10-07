#pragma once
#include <typeindex>
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
#include <typeinfo>

// As RTTI is disabled, we need our own type identification mechanism.
using TypeId = const void*;
template <typename T>
constexpr TypeId getTypeId() {
    // One unique address per distinct T.
    static const int token = 0;
    return &token;
}

//-----------------------------------------------------------------------------
/// @brief Abstract base for all topics
///
/// The _name member must be a string literal and is immutable 
/// after construction.
class TopicBase
{
public:
    explicit TopicBase(const std::string_view n) : _name(n) {}
    virtual ~TopicBase() = default;

    std::string_view getName() const { return _name; }
    virtual TypeId typeId() const = 0;
    virtual bool toJson(char *buf, size_t &len) const = 0;
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
    const std::string_view _name;
};

//-------------------------------------------------------------
/// @brief Typed topic: publishes QMsg<PayloadType>
///
/// Access to the topic is exclusive to one thread at a time.
/// The topic maintains a list of subscribers (IRtosMsgReceiver + msgId).
template <typename PayloadType>
class Topic : public TopicBase
{
    using WriteCb = std::function<bool(const PayloadType &)>;
    using FromJson = std::function<bool(const std::string_view &, PayloadType &)>;
    using ToJson = std::function<bool(const std::string_view &, const PayloadType &, char *, size_t &)>;

public:
    /// @brief Construct a new Topic
    /// @param name Topic name (must be unique and a literal string)
    /// @param cb Optional write callback
    Topic(const std::string_view name, WriteCb cb = nullptr) : TopicBase(name), _writeCb(std::move(cb)), _fromJsonCb(nullptr) {
        
    }

    /// @brief Notify all subscribers of a new message. Can only be called
    /// from the thread that owns the topic.
    /// @return Number of errors encountered during notification, one per failed subscriber.
    size_t notify() override
    {
        if (subscribers_.size() == 0)
        {
            return 0;
        }
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
        if (!_writeCb)
            return false; // writes not supported for this topic
        return _writeCb(value);
    }

    bool requestWrite(const std::string_view &json)
    {
        if (!_fromJsonCb)
            return false; // writes not supported for this topic
        PayloadType value;
        if (!_fromJsonCb(json, value))
            return false;
        return _writeCb ? _writeCb(value) : false;
    }

    /// @brief Set the parse callback for converting json string to payload.
    /// @param cb Write callback that returns true if parsing was successful.
    /// @note Callbacks must be non-throwing.
    void setFromJsonCb(FromJson cb) { _fromJsonCb = std::move(cb); }
    /// @brief Set the to-json callback for converting payload to json string.
    /// @param cb Write callback that returns true if conversion was successful.
    /// @note Callbacks must be non-throwing.
    void setToJsonCb(ToJson cb) { _toJsonCb = std::move(cb); }

    TypeId typeId() const override { return getTypeId<PayloadType>(); }

    bool toJson(char *buffer, size_t &size) const override
    {
        if (!_toJsonCb)
            return false;
        return _toJsonCb(_name, msg.data, buffer, size);
    }

    void setToJsonCb()
    {
        switch (typeId())
        {
        case getTypeId<bool>():
            _toJsonCb = toJsonBool;
            break;
        case getTypeId<int>():
            _toJsonCb = toJsonInt;
            break;
        case getTypeId<float>():
            _toJsonCb = toJsonFloat;
            break;
        }
    }

    static bool toJsonFloat(const std::string_view &name,const float &data, char *buf, size_t &len)
    {
        int written = snprintf(buf, len, "{\"%s\":%f}", name.data(), static_cast<float>(data));
        if (written < 0 || static_cast<size_t>(written) >= len)
            return false;
        len = static_cast<size_t>(written);
        return true;
    }
    static bool toJsonInt(const std::string_view &name,const int &data, char *buf, size_t &len)
    {
        int written = snprintf(buf, len, "{\"%s\":%d}", name.data(), static_cast<int>(data));
        if (written < 0 || static_cast<size_t>(written) >= len)
            return false;
        len = static_cast<size_t>(written);
        return true;
    }
    static bool toJsonBool(const std::string_view &name,const bool &data, char *buf, size_t &len)
    {
        int written = snprintf(buf, len, "{\"%s\":%s}", name.data(), data ? "true" : "false");
        if (written < 0 || static_cast<size_t>(written) >= len)
            return false;
        len = static_cast<size_t>(written);
        return true;
    }

private:
    QMsg<uint32_t, PayloadType> msg;
    WriteCb _writeCb;
    ToJson _toJsonCb;
    FromJson _fromJsonCb;
};

//-----------------------------------------------------------------------------
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
    enum class Result
    {
        OK = 0,
        ZERO_TOPIC,
        TOPIC_EXISTS,
        TOPIC_NOT_FOUND,
        TYPE_MISMATCH,
        SUB_EXISTS,
        SUB_NOT_FOUND,
        WRITE_NOT_SUPPORTED,
        WRITE_FAILED,
        JSON_PARSE_FAILED
    };
    /// @brief Register a topic with the message bus.
    /// @tparam T Payload type
    /// @param topic Pointer to the topic to register. Must not be null.
    /// @return True if registration was successful, false if a topic with
    /// the same name already exists.
    /// @note The topic must remain valid for the lifetime of the MsgBus.
    template <typename T>
    [[nodiscard]] static Result registerTopic(Topic<T> *topic)
    {
        if (!topic)
            return Result::ZERO_TOPIC;
        std::lock_guard<std::mutex> lock(mutex_);
        auto name = std::string(topic->getName());
        auto [it, inserted] = topics_.emplace(std::move(name), topic);
        return inserted ? Result::OK : Result::TOPIC_EXISTS;
    }

    static bool topicExists(const std::string_view name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return topics_.find(name.data()) != topics_.end();
    }

    /// @brief Subscribe a receiver to a topic.
    /// @param name Topic name
    /// @param receiver Receiver message buffer
    /// @param msgId Message ID
    /// @return True if subscription was successful, false otherwise.
    /// @note Before a subscriber is deleted, it must unsubscribe from all topics.
    /// Otherwise, the MsgBus will hold a dangling pointer.
    static Result subscribe(const std::string_view name, IRtosMsgReceiver &receiver, uint32_t msgId)
    {
        TopicBase *topic = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name.data());
            if (it == topics_.end())
                return Result::TOPIC_NOT_FOUND;
            topic = it->second; // safe: topics are never unregistered
        }
        return topic->addSubscriber(receiver, msgId) ? Result::OK : Result::SUB_EXISTS;
    }

    /// @brief Unsubscribe a receiver from a topic.
    /// @param name Topic name
    /// @param receiver Receiver message buffer
    /// @param msgId Message ID
    /// @return True if unsubscription was successful, false otherwise.
    static Result unsubscribe(const std::string_view name, IRtosMsgReceiver &receiver, uint32_t msgId)
    {
        TopicBase *topic = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name.data());
            if (it == topics_.end())
                return Result::TOPIC_NOT_FOUND;
            topic = it->second; // safe: topics are never unregistered
        }
        return topic->removeSubscriber(receiver, msgId) ? Result::OK : Result::SUB_NOT_FOUND;
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
    [[nodiscard]] static Result requestWrite(const std::string_view name, const T &value)
    {
        TopicBase *base = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name.data());
            if (it == topics_.end())
                return Result::TOPIC_NOT_FOUND;
            base = it->second;
        }
        if (base->typeId() == getTypeId<T>())
        {
            return static_cast<Topic<T> *>(base)->requestWrite(value) ? Result::OK : Result::WRITE_FAILED;
        }
        return Result::TYPE_MISMATCH;
    }

    /// @brief Get a JSON representation of the topic's payload.
    /// @param name Topic name
    /// @param json Buffer to write JSON string to.
    /// @param len On input, size of the buffer. On output, number of bytes
    /// @return True if conversion was successful, false otherwise.
    static Result toJson(const std::string_view name, char *json, size_t &len)
    {
        TopicBase *base = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(name.data());
            if (it == topics_.end())
                return Result::TOPIC_NOT_FOUND;
            base = it->second;
        }
        if (base->toJson(json, len))
            return Result::OK;
        return Result::JSON_PARSE_FAILED;
    }
    

    std::string_view resultToString(Result r)
    {
        switch (r)
        {
        case Result::OK:
            return "OK";
        case Result::ZERO_TOPIC:
            return "ZERO_TOPIC";
        case Result::TOPIC_EXISTS:
            return "TOPIC_EXISTS";
        case Result::TOPIC_NOT_FOUND:
            return "TOPIC_NOT_FOUND";
        case Result::TYPE_MISMATCH:
            return "TYPE_MISMATCH";
        case Result::SUB_EXISTS:
            return "SUB_EXISTS";
        case Result::SUB_NOT_FOUND:
            return "SUB_NOT_FOUND";
        case Result::WRITE_NOT_SUPPORTED:
            return "WRITE_NOT_SUPPORTED";
        case Result::WRITE_FAILED:
            return "WRITE_FAILED";
        default:
            return "UNKNOWN";
        }
    }

private:
    inline static std::map<std::string, TopicBase *> topics_{}; // or unordered_map
    inline static std::mutex mutex_;
};
