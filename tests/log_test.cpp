#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "rtos/Log.hpp"

namespace
{
class LogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rtos::Log::clearSinks();
        rtos::Log::setGlobalLevel(rtos::LogLevel::Info);
    }

    void TearDown() override
    {
        rtos::Log::clearSinks();
    }
};

class CapturingSink final : public rtos::ILogSink
{
public:
    void write(
        rtos::LogLevel level,
        const char *tag,
        const char *line,
        std::size_t len) override
    {
        levels.push_back(level);
        tags.emplace_back(tag ? tag : "");
        lines.emplace_back(line, len);
    }

    std::vector<rtos::LogLevel> levels;
    std::vector<std::string> tags;
    std::vector<std::string> lines;
};

class SelfClearingSink final : public rtos::ILogSink
{
public:
    explicit SelfClearingSink(std::promise<void> &completed)
        : completed(completed)
    {
    }

    void write(
        rtos::LogLevel,
        const char *,
        const char *,
        std::size_t) override
    {
        rtos::Log::clearSinks();
        completed.set_value();
    }

private:
    std::promise<void> &completed;
};

class RecursiveSink final : public rtos::ILogSink
{
public:
    void write(
        rtos::LogLevel,
        const char *,
        const char *,
        std::size_t) override
    {
        ++calls;
        rtos::Log::log(rtos::LogLevel::Info, "recursive", "nested");
    }

    std::atomic<unsigned> calls{0};
};
}

TEST_F(LogTest, FansOutStructuredLogMetadata)
{
    CapturingSink first;
    CapturingSink second;
    rtos::Log::addSink(first);
    rtos::Log::addSink(second);

    rtos::Log::log(rtos::LogLevel::Info, "device", "hello %d", 7);

    ASSERT_EQ(first.lines.size(), 1u);
    ASSERT_EQ(second.lines.size(), 1u);
    EXPECT_EQ(first.levels.front(), rtos::LogLevel::Info);
    EXPECT_EQ(first.tags.front(), "device");
    EXPECT_NE(first.lines.front().find("hello 7"), std::string::npos);
    EXPECT_EQ(second.lines.front(), first.lines.front());
}

TEST_F(LogTest, SinkCanClearRegistryFromCallback)
{
    std::promise<void> completed;
    auto completion = completed.get_future();
    SelfClearingSink sink(completed);
    rtos::Log::addSink(sink);

    std::thread logger([] {
        rtos::Log::log(rtos::LogLevel::Info, "device", "clear");
    });

    const auto status = completion.wait_for(std::chrono::milliseconds(250));
    if (status == std::future_status::ready)
    {
        logger.join();
    }
    else
    {
        logger.detach();
    }
    EXPECT_EQ(status, std::future_status::ready);
}

TEST_F(LogTest, RecursiveSinkDispatchIsSuppressed)
{
    RecursiveSink sink;
    rtos::Log::addSink(sink);

    std::promise<void> completed;
    auto completion = completed.get_future();
    std::thread logger([&completed] {
        rtos::Log::log(rtos::LogLevel::Info, "device", "outer");
        completed.set_value();
    });

    const auto status = completion.wait_for(std::chrono::milliseconds(250));
    if (status == std::future_status::ready)
    {
        logger.join();
    }
    else
    {
        logger.detach();
    }
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(sink.calls.load(), 1u);
}

namespace
{
class RecordSink final : public rtos::ILogSink
{
public:
    void write(rtos::LogLevel, const char *, const char *, std::size_t) override
    {
        ++legacyWrites;
    }

    void writeRecord(const rtos::LogRecord &record) override
    {
        levels.push_back(record.level);
        tags.emplace_back(record.tag);
        bodies.emplace_back(record.body, record.bodyLen);
        lines.emplace_back(record.line, record.lineLen);
    }

    unsigned legacyWrites = 0;
    std::vector<rtos::LogLevel> levels;
    std::vector<std::string> tags;
    std::vector<std::string> bodies;
    std::vector<std::string> lines;
};
}

TEST_F(LogTest, RecordSinkGetsTheBareBodyBesideTheFormattedLine)
{
    // A sink that overrides writeRecord() sees the message before the rtos prefix is put on
    // it (what EspIdfLogSink hands to esp_log, HMO-86) and the prefixed line side by side,
    // and the legacy write() is not called for it.
    RecordSink record;
    CapturingSink legacy;
    rtos::Log::addSink(record);
    rtos::Log::addSink(legacy);

    rtos::Log::log(rtos::LogLevel::Warn, "RmtReceiver", "Unexpected RMT levels: %d -> %d", 1, 1);

    ASSERT_EQ(record.bodies.size(), 1u);
    EXPECT_EQ(record.levels.front(), rtos::LogLevel::Warn);
    EXPECT_EQ(record.tags.front(), "RmtReceiver");
    EXPECT_EQ(record.bodies.front(), "Unexpected RMT levels: 1 -> 1");
    EXPECT_EQ(record.lines.front(), legacy.lines.front());
    EXPECT_NE(record.lines.front().find("W/RmtReceiver: Unexpected RMT levels: 1 -> 1"),
              std::string::npos);
    EXPECT_EQ(record.legacyWrites, 0u);
}
