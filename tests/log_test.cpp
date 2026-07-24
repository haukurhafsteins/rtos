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
