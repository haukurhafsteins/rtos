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
#include "esp_log.h"
#include "Metrics.hpp"

// As RTTI is disabled, we need our own type identification mechanism.
using TypeId = const void *;
using TopicId = uint32_t;

template <typename T>
constexpr TypeId getTypeId()
{
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
    explicit TopicBase(const std::string_view n) : _name(n), topicUnit(Metrics::Unit::none), topicId(fnv1a32(n)) {}
    virtual ~TopicBase() = default;

    std::string_view getName() const { return _name; }
    virtual TypeId typeId() const = 0;
    virtual int toJson(std::span<char> json, const std::span<const std::byte> buffer, const char *format) const = 0;
    virtual int toJson(std::span<char> json) const = 0;
    virtual bool requestWrite(const std::string_view &json) = 0;
    [[nodiscard]] virtual size_t notify() = 0;

    /// @brief Add a subscriber to the topic.
    /// @param q Message queue to subscribe.
    /// @param topicId Topic ID to subscribe to.
    /// @return True if the subscriber was added, false if it already exists.
    [[nodiscard]] bool addSubscriber(IRtosMsgReceiver &q, TopicId topicId)
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        // printf("Adding subscriber to topic %s, queue: %p topicId: %lu\n", _name.data(), &q, topicId);
        auto it = find_if(subscribers_.begin(), subscribers_.end(),
                          [&](const Sub &s)
                          { return s.q == &q && s.id == topicId; });
        if (it == subscribers_.end())
        {
            subscribers_.push_back(Sub{&q, topicId});
            return true;
        }
        return false;
    }

    /// @brief Remove a subscriber from the topic.
    /// @note Must be called before the subscriber is deleted.
    /// @param q Message queue to unsubscribe.
    /// @param topicId Topic ID to unsubscribe from.
    /// @return True if the subscriber was removed, false if it did not exist.
    [[nodiscard]] bool removeSubscriber(IRtosMsgReceiver &q, TopicId topicId)
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        auto it = remove_if(subscribers_.begin(), subscribers_.end(),
                            [&](const Sub &s)
                            { return s.q == &q && s.id == topicId; });
        if (it != subscribers_.end())
        {
            subscribers_.erase(it, subscribers_.end());
            return true;
        }
        return false;
    }

    constexpr static TopicId INVALID_TOPIC_ID = 0;

    static constexpr TopicId fnv1a32(std::string_view s)
    {
        uint32_t h = 0x811C9DC5u;
        for (unsigned char c : s)
        {
            h ^= c;
            h *= 0x01000193u;
        }
        return static_cast<TopicId>(h);
    }

    static bool fromJsonBool(const std::string_view &json, bool &outValue)
    {
        if (json == "true" || json == "1")
        {
            outValue = true;
            return true;
        }
        else if (json == "false" || json == "0")
        {
            outValue = false;
            return true;
        }
        return false;
    }

    TopicId getId() const { return topicId; }
    const Metrics::Unit &getUnit() const { return topicUnit; }
    const std::string &getFormat() const { return format; }
    void setUnit(Metrics::Unit unit) { topicUnit = unit; }
    void setFormat(const std::string &fmt) { format = fmt; }
    size_t subscribers() const
    {
        std::lock_guard<std::mutex> lk(subs_mtx_);
        return subscribers_.size();
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
    Metrics::Unit topicUnit;
    TopicId topicId;
    std::string format{""};
};

//-------------------------------------------------------------
/// @brief Typed topic: publishes QMsg<T>
///
/// Access to the topic is exclusive to one thread at a time.
/// The topic maintains a list of subscribers (IRtosMsgReceiver and topicId).
template <typename T, Metrics::Unit U = Metrics::Unit::none>
class Topic : public TopicBase
{
    using WriteCb = std::function<bool(const T &)>;
    using FromJson = std::function<bool(const std::string_view &, T &)>;
    /// @brief Callback to convert payload to json string.
    /// @param [in] value Current value
    /// @param [out] json Buffer to write json string to.
    using ToJson = std::function<int(const T &value, std::span<char> json, const char *format)>;

public:
    /// @brief Construct a new Topic
    /// @param name Topic name (must be unique and a literal string)
    /// @param cb Optional write callback
    Topic(const std::string_view name, WriteCb cb = nullptr) : TopicBase(name), _writeCb(std::move(cb)), _toJsonCb(nullptr), _fromJsonCb(nullptr) {}

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
    T &data() { return msg.getData(); }

    /// @brief Request a write to the topic. Returns false if writes are not
    /// supported or if the write callback returns false.
    /// @param value Payload value
    /// @note This does NOT notify subscribers. The owner of the topic will
    /// need to call notify() after validating and applying the write.
    /// @note Callbacks must be non-throwing.
    bool requestWrite(const T &value)
    {
        if (!_writeCb)
            return false; // writes not supported for this topic
        return _writeCb(value);
    }

    bool requestWrite(const std::string_view &json) override
    {
        if (!_fromJsonCb)
            return false; // writes not supported for this topic
        T value;
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

    TypeId typeId() const override { return getTypeId<T>(); }

    int toJson(std::span<char> json, const std::span<const std::byte> buffer, const char *format) const override
    {
        if (!_toJsonCb)
            return -1;
        const T *data = reinterpret_cast<const T *>(buffer.data());
        return _toJsonCb(*data, json, format);
    }
    int toJson(std::span<char> json) const override
    {
        if (!_toJsonCb)
            return -1;
        return _toJsonCb(msg.data, json, nullptr);
    }

    static int toJsonFloat(const float &data, std::span<char> json, const char *format)
    {
        int written = snprintf(json.data(), json.size(), format ? format : "%f", static_cast<float>(data));
        if (written < 0 || static_cast<size_t>(written) >= json.size())
            return -1;
        return written;
    }
    static int toJsonInt(const int &data, std::span<char> json, const char *format)
    {
        int written = snprintf(json.data(), json.size(), "%d", static_cast<int>(data));
        if (written < 0 || static_cast<size_t>(written) >= json.size())
            return -1;
        return written;
    }
    static int toJsonBool(const bool &data, std::span<char> json, const char *format)
    {
        int written = snprintf(json.data(), json.size(), "%s", data ? "true" : "false");
        if (written < 0 || static_cast<size_t>(written) >= json.size())
            return -1;
        return written;
    }

private:
    QMsg<uint32_t, T> msg;
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
    struct TopicInfo
    {
        std::string name;
        std::string type;
        size_t subscribers;
    };

    /// @brief Register a topic with the message bus.
    /// @tparam T Payload type
    /// @param topic Pointer to the topic to register. Must not be null.
    /// @return Result::OK if registration was successful, Result::TOPIC_EXISTS if a topic with
    /// the same name already exists.
    /// @note The topic must remain valid for the lifetime of the MsgBus.
    template <typename T>
    static Result registerTopic(Topic<T> *topic, TopicId *outHandle = nullptr)
    {
        if (!topic)
            return Result::ZERO_TOPIC;
        std::lock_guard<std::mutex> lock(mutex_);
        auto name = std::string(topic->getName());
        auto topicHandle = TopicBase::fnv1a32(name);
        auto [it, inserted] = topics_.emplace(topicHandle, topic);
        if (inserted && outHandle)
            *outHandle = topicHandle;
        return inserted ? Result::OK : Result::TOPIC_EXISTS;
    }

    static TopicId topicId(const std::string_view name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto topicId = TopicBase::fnv1a32(name);
        auto it = topics_.find(topicId);
        return it != topics_.end() ? topicId : 0;
    }

    static std::string_view topicName(const TopicId topicId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(topicId);
        return it != topics_.end() ? it->second->getName() : "";
    }

    /// @brief Subscribe a receiver to a topic.
    /// @param name Topic name
    /// @param receiver Receiver message buffer
    /// @return OK if subscription was successful, TOPIC_NOT_FOUND if the topic does not exist,
    /// SUB_EXISTS if the receiver is already subscribed to the topic.
    /// @note Before a subscriber is deleted, it must unsubscribe from all topics.
    /// Otherwise, the MsgBus will hold a dangling pointer.
    static Result subscribe(const TopicId topicId, IRtosMsgReceiver &receiver)
    {
        TopicBase *topic = findTopic(topicId);
        if (!topic)
            return Result::TOPIC_NOT_FOUND;
        return topic->addSubscriber(receiver, topicId) ? Result::OK : Result::SUB_EXISTS;
    }
    static Result subscribe(const std::string_view name, IRtosMsgReceiver &receiver)
    {
        return subscribe(topicId(name), receiver);
    }

    /// @brief Unsubscribe a receiver from a topic.
    /// @param topicId Topic ID
    /// @param receiver Receiver message buffer
    /// @return True if unsubscription was successful, false otherwise.
    static Result unsubscribe(const TopicId topicId, IRtosMsgReceiver &receiver)
    {
        TopicBase *topic = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = topics_.find(topicId);
            if (it == topics_.end())
                return Result::TOPIC_NOT_FOUND;
            topic = it->second; // safe: topics are never unregistered
        }
        return topic->removeSubscriber(receiver, topicId) ? Result::OK : Result::SUB_NOT_FOUND;
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
    static Result requestWrite(const TopicId topicId, const T &value)
    {
        TopicBase *topic = findTopic(topicId);
        if (!topic)
            return Result::TOPIC_NOT_FOUND;
        if (topic->typeId() != getTypeId<T>())
            return Result::TYPE_MISMATCH;
        return static_cast<Topic<T> *>(topic)->requestWrite(value) ? Result::OK : Result::WRITE_FAILED;
    }
    template <typename T>
    static Result requestWrite(const std::string_view name, const T &value)
    {
        const TopicId topicId = MsgBus::topicId(name);
        if (topicId == 0)
            return Result::TOPIC_NOT_FOUND;
        return requestWrite<T>(topicId, value);
    }

    static Result requestWrite(const TopicId topicId, const std::string_view json)
    {
        TopicBase *topic = findTopic(topicId);
        if (!topic)
            return Result::TOPIC_NOT_FOUND;
        return topic->requestWrite(json) ? Result::OK : Result::WRITE_FAILED;
    }

    /// @brief Get a JSON representation of the topic's payload.
    /// @param handle Topic handle
    /// @param buffer Buffer containing the payload data.
    /// @param json Buffer to write JSON string to.
    /// @return Result::OK if successful, Result::TOPIC_NOT_FOUND if the topic does not exist,
    static Result toJson(const TopicId topicId, const std::span<const std::byte> buffer, std::span<char> json)
    {
        const TopicBase *topic = findTopic(topicId);
        if (!topic)
            return Result::TOPIC_NOT_FOUND;
        if (topic->toJson(json, buffer, nullptr))//TODO: format doesnt exist for some parameters. topic->getFormat().c_str()) > 0)
            return Result::OK;
        return Result::JSON_PARSE_FAILED;
    }
    static Result toJson(const TopicId topicId, std::span<char> json)
    {
        const TopicBase *topic = findTopic(topicId);
        if (!topic)
            return Result::TOPIC_NOT_FOUND;
        if (topic->toJson(json))
            return Result::OK;
        return Result::JSON_PARSE_FAILED;
    }

    static std::string_view resultToString(Result r)
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
        case Result::JSON_PARSE_FAILED:
            return "JSON_PARSE_FAILED";
        default:
            printf("MsgBus::resultToString: Unknown result %d\n", static_cast<int>(r));
            return "UNKNOWN";
        }
    }

    static void getTopicList(std::vector<TopicId> &outList)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[handle, topic] : topics_)
        {
            outList.push_back(handle);
        }
    }

    static Result getTopicInfo(const TopicId topic, TopicInfo &info)
    {
        info.name = std::string(topicName(topic));
        TopicBase *topicPtr = findTopic(topic);
        if (!topicPtr)
            return Result::TOPIC_NOT_FOUND;
        // RTTI is disabled, so we cannot use typeid; return a safe default.
        info.type = "unknown";
        info.subscribers = topicPtr->subscribers();
        return Result::OK;
    }

private:
    static TopicBase *findTopic(const TopicId handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = topics_.find(handle);
        if (it == topics_.end())
            return nullptr;
        return it->second;
    }
    inline static std::map<TopicId, TopicBase *> topics_{}; // or unordered_map
    inline static std::mutex mutex_;
};
