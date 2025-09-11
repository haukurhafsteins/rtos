#pragma once
#include <map>
#include <list>
#include <string>
#include <cstdint>
#include "RtosMsgBuffer.hpp"
#include "QMsg.hpp"

//--------------------------------------------
// Abstract base for all topics
class TopicBase {
public:
    explicit TopicBase(const char* n) : name_(n) {}
    virtual ~TopicBase() = default;

    const char* getName() const { return name_; }
    virtual bool notify() = 0;

    void addSubscriber(RtosMsgBuffer& q, uint32_t msgId) {
        subs_.push_back({ &q, msgId });
    }
    void removeSubscriber(RtosMsgBuffer& q) {
        subs_.remove_if([&](const Sub& s){ return s.q == &q; });
    }

protected:
    struct Sub { RtosMsgBuffer* q; uint32_t id; };
    std::list<Sub> subs_;

private:
    const char* name_;
};

//--------------------------------------------
// Typed topic: publishes QMsg<Cmd, T>
template<typename Cmd, typename T>
class Topic : public TopicBase {
public:
    Topic(const char* name, const T* dataPtr)
        : TopicBase(name), data_(dataPtr) {}

    const T* getData() const { return data_; }

    bool notify() override {
        if (!data_) return false;
        for (auto& s : this->subs_) {
            QMsg<Cmd, T> msg(static_cast<Cmd>(s.id), *data_);
            s.q->send(&msg, msg.size());
        }
        return true;
    }

private:
    const T* data_; // external storage
};

//--------------------------------------------
// Registry: central message bus
class MsgBus {
public:
    template<typename Cmd, typename T>
    static TopicBase* add(const char* name, const T* data) {
        if (topics_.find(name) != topics_.end()) return nullptr;
        auto* t = new Topic<Cmd, T>(name, data);
        topics_[name] = t;
        return t;
    }

    static bool remove(const char* name) {
        auto it = topics_.find(name);
        if (it == topics_.end()) return false;
        delete it->second;
        topics_.erase(it);
        return true;
    }

    template<typename C>
    static bool subscribe(const char* name, RtosMsgBuffer& receiver, C msgId) {
        auto it = topics_.find(name);
        if (it == topics_.end()) return false;
        it->second->addSubscriber(receiver, static_cast<uint32_t>(msgId));
        return true;
    }

    static TopicBase* get(const char* name) {
        auto it = topics_.find(name);
        return (it == topics_.end()) ? nullptr : it->second;
    }

private:
    inline static std::map<std::string, TopicBase*> topics_{};
};
