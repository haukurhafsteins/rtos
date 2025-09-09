# Message Bus: `TopicBase`, `Topic<T,Cmd>`, and `MsgBus`

A tiny, header-only publish/subscribe message bus for RTOS-style systems. It lets producers publish typed payloads to a named **topic**, and lets consumers subscribe with their own command IDs. Delivery uses a user-provided `RtosMsgBuffer` queue and a simple `QMsg<Cmd, T>` envelope.

> **Requires:** C++17 (uses `inline static`), a `RtosMsgBuffer` with `send(void*, size_t)` and a `QMsg<Cmd,T>` type exposing `size()`.

---

## Overview

- **Topics are named.** Each topic holds a pointer to external data (`const T*`) owned by the producer.
- **Publishing = `notify()`.** When called, the topic wraps the current `T` into `QMsg<Cmd,T>` and sends it to each subscriber’s `RtosMsgBuffer`.
- **Subscribers register per-topic.** Each subscriber chooses its own command ID (cast to `uint32_t`). The same topic can send different command IDs to different subscribers.
- **Central registry.** `MsgBus` keeps a global map from topic name → `TopicBase*`.

---

## Class & API Reference

### `TopicBase`
Abstract base for all topics.

```cpp
class TopicBase {
public:
    explicit TopicBase(const char* n);
    virtual ~TopicBase() = default;

    const char* getName() const;
    virtual bool notify() = 0;

    void addSubscriber(RtosMsgBuffer& q, uint32_t msgId);
    void removeSubscriber(RtosMsgBuffer& q);

protected:
    struct Sub { RtosMsgBuffer* q; uint32_t id; };
    std::list<Sub> subs_;
};
```

- **`getName()`** – Returns the topic’s name.
- **`notify()`** – Pure virtual; derived classes send the current payload to all subscribers. Returns `false` if nothing was sent.
- **`addSubscriber(queue, id)`** – Adds a subscriber with a message/command ID.
- **`removeSubscriber(queue)`** – Removes *all* subscriptions for the given queue.

**Complexity**
- `removeSubscriber`: linear in number of subs (list filter).
- `notify`: linear in number of subs.

---

### `Topic<T, Cmd>`
Typed topic; publishes `QMsg<Cmd, T>`.

```cpp
template<typename T, typename Cmd>
class Topic : public TopicBase {
public:
    Topic(const char* name, const T* dataPtr);

    const T* getData() const;   // returns the external data pointer
    bool notify() override;     // sends QMsg<Cmd,T> to each subscriber

private:
    const T* data_; // external storage (must outlive the topic)
};
```

- **Data ownership:** `Topic` does **not** own `T`. You pass a pointer to state you maintain elsewhere; keep it valid for the topic’s lifetime.
- **`notify()` logic:** For each subscriber, constructs `QMsg<Cmd,T> msg(static_cast<Cmd>(s.id), *data_)` and calls `s.q->send(&msg, msg.size())`. Returns `false` if `data_` is `nullptr`, `true` otherwise.

> Ensure `Cmd` is constructible from `uint32_t` (commonly an enum/enum class with `uint32_t` underlying type).

---

### `MsgBus`
Static registry of topics by name.

```cpp
class MsgBus {
public:
    template<typename T, typename Cmd, typename C>
    static TopicBase* add(const char* name, C /*id*/, const T* data);

    static bool remove(const char* name);

    template<typename C>
    static bool subscribe(const char* name, RtosMsgBuffer& receiver, C msgId);

    static TopicBase* get(const char* name);

private:
    inline static std::map<std::string, TopicBase*> topics_{};
};
```

- **`add<T, Cmd, C>(name, id, data)`** – Creates and registers a `Topic<T,Cmd>`.
  - Returns the created `TopicBase*` or `nullptr` if a topic with the same name already exists.
  - The `id` parameter is **not used at runtime**; it exists to help the compiler deduce/validate a command type at the call site.
- **`remove(name)`** – Unregisters and deletes the topic. Returns `true` on success.
- **`subscribe(name, queue, msgId)`** – Subscribes a receiver to a named topic with the given command ID (any type convertible to `uint32_t`). Returns `true` if topic exists.
- **`get(name)`** – Returns the topic pointer or `nullptr` if not found.

**Complexity**
- Name lookups are `O(log N)` (map).
- `add/remove` are `O(log N)` + allocation/deletion.

---

## Typical Usage

### 1) Define your payload and command enum

```cpp
enum class SensorCmd : uint32_t {
    DataUpdated = 1,
    // ... other commands
};

struct Temperature {
    float celsius;
    uint64_t timestamp_us;
};
```

> `QMsg<Cmd,T>` must be constructible as `QMsg<Cmd,T>(Cmd, const T&)` and provide `size()`.

### 2) Create the topic

```cpp
static Temperature temp_state{ 0.f, 0 };

auto* topicTemperature = MsgBus::add<Temperature, SensorCmd>(
    "temp", SensorCmd::DataUpdated, &temp_state
);

// Optionally keep a typed handle if you need `getData()`:
auto* tempTopic = static_cast<Topic<Temperature, SensorCmd>*>(base);
```

### 3) Subscribe receivers (usually tasks) with their queues

```cpp
extern RtosMsgBuffer tempConsumerQueue;

bool ok = MsgBus::subscribe("temp", tempConsumerQueue, SensorCmd::DataUpdated);
```

> Different subscribers can pick different IDs (e.g., `DataUpdated` vs `AlarmRaised`) even on the same topic.

### 4) Publish updates (`notify()`)

Update your external state, then notify:

```cpp
temp_state.celsius = read_sensor();
temp_state.timestamp_us = now_us();

// Use the topicTemperature variable directly to trigger notification
topicTemperature->notify();
// or get the parameter from the bus and trigger notification
if (auto* t = MsgBus::get("temp")) {
    t->notify(); // broadcasts QMsg<SensorCmd, Temperature> to all subs
}
```

### 5) Unsubscribe and cleanup

```cpp
if (auto* t = MsgBus::get("temp")) {
    t->removeSubscriber(tempConsumerQueue);
}

MsgBus::remove("temp"); // deletes the topic from the registry
```

---

## Design Notes & Guarantees

- **Zero-copy payload pointer:** The topic reads `*data_` by value into the message at publish time. Keep `data_` valid and coherent during `notify()`.
- **Command ID per subscriber:** Each subscription stores its own ID. On publish, that ID is cast to `Cmd` for the outgoing `QMsg`. This lets the same topic drive different command handlers.
- **Minimal coupling:** Publishers don’t know about receivers; receivers don’t know the payload type beyond what `QMsg` encodes.
- **Ownership & lifetime:** `MsgBus::add` `new`s a `Topic`. `MsgBus::remove` deletes it. Don’t leak topics—always remove them before shutdown or re-registration.

---

## Thread-Safety & RTOS Considerations

- **No internal locking.** `MsgBus` and `Topic` are **not** thread-safe. If multiple tasks add/remove/subscribe/notify concurrently, protect with a mutex.
- **Queue backpressure:** `notify()` ignores `RtosMsgBuffer::send` return value. If your queue can block/fail, consider extending `notify()` or `RtosMsgBuffer` to report and handle failures.
- **ISR context:** As written, `notify()` constructs a stack `QMsg` and calls `send`. Only use from ISR if your queue supports it and `send` is ISR-safe. Otherwise, call from task context.

---

## Error Handling

- `MsgBus::add` → `nullptr` if name already used.
- `MsgBus::subscribe` → `false` if topic missing.
- `MsgBus::remove` → `false` if topic missing.
- `Topic::notify` → `false` if `data_ == nullptr`, else `true`.
- Delivery failures from `RtosMsgBuffer::send` are **not** propagated.

---

## Best Practices

- **Use `enum class` for `Cmd`** with `uint32_t` underlying type for clean casts and strong typing.
- **Keep topics long-lived.** Create at init, remove at shutdown. Avoid frequent add/remove churn.
- **Name topics hierarchically,** e.g., `"sensors/temperature"`, `"ui/buttonA"`.
- **Document payload stability.** Receivers should know if fields are always set, units, timestamp domain, etc.
- **Consider a typed accessor** if you frequently need `getData()`:
  ```cpp
  template<class T, class Cmd>
  Topic<T,Cmd>* topic_cast(TopicBase* b) {
      return static_cast<Topic<T,Cmd>*>(b);
  }
  ```

---

## Example End-to-End

```cpp
// Commands
enum class SensorCmd : uint32_t { DataUpdated = 1 };

// Payload
struct Temperature {
    float celsius;
    uint64_t ts_us;
};

// Global state
static Temperature g_temp{ 0.f, 0 };

// Create topic at startup
auto* topicTemperature = MsgBus::add<Temperature, SensorCmd>("temp", SensorCmd::DataUpdated, &g_temp);
assert(topicTemperature && "topic name already in use");

// Subscribe a consumer
extern RtosMsgBuffer tempQueue;
bool subOk = MsgBus::subscribe("temp", tempQueue, SensorCmd::DataUpdated);
assert(subOk);

// Producer loop
void temp_task() {
    for (;;) {
        g_temp.celsius = read_sensor_c();
        g_temp.ts_us   = time_us();
        topicTemperature->notify();
        delay_ms(100);
    }
}

// Unsubscribe & cleanup at shutdown
void shutdown() {
    if (auto* t = MsgBus::get("temp")) {
        t->removeSubscriber(tempQueue);
    }
    MsgBus::remove("temp");
}
```

---

## Limitations & Extensions

- **No typed lookup.** `get(name)` returns `TopicBase*`. If you need typed operations beyond `notify()`/`removeSubscriber`, cast to `Topic<T,Cmd>`.
- **No retention / last-value cache** beyond your external `T`. If consumers need the latest value on demand, expose `getData()` via a typed pointer.
- **No wildcard topics or filters.** Subscriptions are exact by name.
- **No metrics/backpressure handling.** Consider adding `notify()` overloads that propagate send status, or a `for_each_subscriber` hook.

---

## FAQ

**Q: Why does `MsgBus::add` take an unused `id` parameter?**  
A: It acts as a *type tag* to help template argument deduction or enforce that your command enumeration type is present at the call site. It isn’t used at runtime.

**Q: Can different subscribers use different command IDs on the same topic?**  
A: Yes—that’s a core feature. Each subscriber’s stored `id` becomes the `Cmd` value for its messages.

**Q: Is it safe to update `*data_` while calling `notify()`?**  
A: Avoid concurrent mutation. Update `*data_` before `notify()`, or guard with a mutex so the `QMsg` sees a consistent snapshot.

---

## Summary

This bus gives you a **minimal, allocation-light** pattern for fan-out messaging: *update your state → `notify()` → each subscriber gets a `QMsg<Cmd,T>`*. You keep ownership of the payload, and the registry keeps topics discoverable by name. Add locking and delivery checks as your system grows.
