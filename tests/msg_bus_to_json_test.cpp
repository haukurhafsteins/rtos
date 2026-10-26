// Regression tests for HMO-143.
//
// TopicBase::toJson used to return a bare int, with -1 for "this topic has no toJson
// callback". Both MsgBus::toJson wrappers tested that int for truth rather than
// comparing it, and `if (-1)` is true, so the one failure the wrappers exist to catch
// was reported as Result::OK and the caller emitted whatever was in the buffer.
//
// These tests fail if the comparison regresses in either overload.

#include "rtos/MsgBus.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>

namespace
{

using rtos::JsonResult;
using rtos::MsgBus;
using rtos::Topic;
using rtos::TopicId;

// Topics are registered once for the lifetime of the bus and are never unregistered,
// so every test needs its own topic name.
TopicId registerFloat(Topic<float> &topic)
{
    TopicId id = 0;
    EXPECT_EQ(MsgBus::registerTopic<float>(&topic, &id), MsgBus::Result::OK);
    EXPECT_NE(id, 0u);
    return id;
}

std::span<const std::byte> asBytes(const float &v)
{
    return std::span<const std::byte>(reinterpret_cast<const std::byte *>(&v), sizeof(v));
}

} // namespace

//-----------------------------------------------------------------------------
// A topic with no toJson callback must report failure, not OK.

TEST(MsgBusToJson, BufferOverloadRejectsTopicWithoutCallback)
{
    Topic<float> topic("test/no_cb/buffer");
    const TopicId id = registerFloat(topic);

    std::array<char, 32> json{};
    json[0] = '\0';
    const float payload = 1.5f;

    EXPECT_EQ(MsgBus::toJson(id, asBytes(payload), json), MsgBus::Result::JSON_NOT_SUPPORTED);
    EXPECT_STREQ(json.data(), "");
}

TEST(MsgBusToJson, ValueOverloadRejectsTopicWithoutCallback)
{
    Topic<float> topic("test/no_cb/value");
    const TopicId id = registerFloat(topic);

    std::array<char, 32> json{};
    json[0] = '\0';

    EXPECT_EQ(MsgBus::toJson(id, json), MsgBus::Result::JSON_NOT_SUPPORTED);
    EXPECT_STREQ(json.data(), "");
}

// The callback-less case is distinguishable from a conversion that failed, so a caller
// can tell a setup mistake from a value that would not render.
TEST(MsgBusToJson, NoCallbackIsDistinctFromParseFailure)
{
    EXPECT_NE(MsgBus::Result::JSON_NOT_SUPPORTED, MsgBus::Result::JSON_PARSE_FAILED);
    EXPECT_NE(MsgBus::Result::JSON_NOT_SUPPORTED, MsgBus::Result::OK);
    EXPECT_EQ(MsgBus::resultToString(MsgBus::Result::JSON_NOT_SUPPORTED), "JSON_NOT_SUPPORTED");
}

//-----------------------------------------------------------------------------
// A callback that reports failure must also not be reported as OK. This is the other
// half of the same bug: toJsonFloat returns -1 when the value does not fit, and `if (-1)`
// let a truncated render through as success.

TEST(MsgBusToJson, BufferOverloadRejectsCallbackFailure)
{
    Topic<float> topic("test/fail_cb/buffer");
    const TopicId id = registerFloat(topic);
    topic.setToJsonCb(Topic<float>::toJsonFloat);

    std::array<char, 4> json{}; // too small for "%f" of 1234.5
    const float payload = 1234.5f;

    EXPECT_EQ(MsgBus::toJson(id, asBytes(payload), json), MsgBus::Result::JSON_PARSE_FAILED);
}

TEST(MsgBusToJson, ValueOverloadRejectsCallbackFailure)
{
    Topic<float> topic("test/fail_cb/value");
    const TopicId id = registerFloat(topic);
    topic.setToJsonCb(Topic<float>::toJsonFloat);
    topic.value() = 1234.5f;

    std::array<char, 4> json{};

    EXPECT_EQ(MsgBus::toJson(id, json), MsgBus::Result::JSON_PARSE_FAILED);
}

// A callback that writes nothing is a failure too: an empty render is not a JSON value,
// and it is exactly what produced `{"name":...,"value":}` on the wire.
TEST(MsgBusToJson, ZeroBytesWrittenIsAFailure)
{
    Topic<float> topic("test/zero_cb/value");
    const TopicId id = registerFloat(topic);
    topic.setToJsonCb([](const float &, std::span<char> json, const char *) -> int
                      {
        if (!json.empty())
            json[0] = '\0';
        return 0; });

    std::array<char, 32> json{};

    EXPECT_EQ(MsgBus::toJson(id, json), MsgBus::Result::JSON_PARSE_FAILED);
}

//-----------------------------------------------------------------------------
// The success path still carries the byte count the callbacks return.

TEST(MsgBusToJson, SuccessReportsBytesWritten)
{
    Topic<float> topic("test/ok_cb/value");
    const TopicId id = registerFloat(topic);
    topic.setToJsonCb(Topic<float>::toJsonFloat);
    topic.value() = 2.0f;

    std::array<char, 32> json{};
    EXPECT_EQ(MsgBus::toJson(id, json), MsgBus::Result::OK);

    const JsonResult direct = topic.toJson(json);
    ASSERT_TRUE(direct.ok());
    EXPECT_EQ(direct.error(), JsonResult::Error::none);
    EXPECT_EQ(direct.written(), static_cast<int>(std::strlen(json.data())));
    EXPECT_GT(direct.written(), 0);
}

TEST(MsgBusToJson, BufferOverloadSuccessReportsBytesWritten)
{
    Topic<float> topic("test/ok_cb/buffer");
    const TopicId id = registerFloat(topic);
    topic.setToJsonCb(Topic<float>::toJsonFloat);

    std::array<char, 32> json{};
    const float payload = 3.25f;
    EXPECT_EQ(MsgBus::toJson(id, asBytes(payload), json), MsgBus::Result::OK);

    const JsonResult direct = topic.toJson(json, asBytes(payload), nullptr);
    ASSERT_TRUE(direct.ok());
    EXPECT_EQ(direct.written(), static_cast<int>(std::strlen(json.data())));
}

//-----------------------------------------------------------------------------
// The failure states report no byte count, so a caller that reads written() without
// checking ok() cannot mistake a failure for a short write.

TEST(MsgBusToJson, FailureStatesReportNoBytesWritten)
{
    const JsonResult noCb = JsonResult::noCallback();
    EXPECT_FALSE(noCb.ok());
    EXPECT_EQ(noCb.error(), JsonResult::Error::noCallback);
    EXPECT_EQ(noCb.written(), 0);

    const JsonResult failed = JsonResult::conversionFailed();
    EXPECT_FALSE(failed.ok());
    EXPECT_EQ(failed.error(), JsonResult::Error::conversionFailed);
    EXPECT_EQ(failed.written(), 0);

    // The shape of the original bug: the callback's -1 must not survive as a truthy value.
    EXPECT_FALSE(JsonResult::fromCallback(-1).ok());
    EXPECT_FALSE(JsonResult::fromCallback(0).ok());
    EXPECT_TRUE(JsonResult::fromCallback(1).ok());
}

// JsonResult must not be usable as a condition. If this ever compiles with
// `if (result)`, the bug this ticket fixed is back.
static_assert(!std::is_convertible_v<rtos::JsonResult, bool>,
              "JsonResult must not convert to bool: `if (topic->toJson(...))` is HMO-143");
