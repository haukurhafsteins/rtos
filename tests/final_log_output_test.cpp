#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "esp_log.h"
#include "rtos/FinalLogOutput.hpp"

namespace
{
vprintf_like_t s_outputFunction = nullptr;
std::mutex s_priorMutex;
std::string s_priorOutput;

int priorOutput(const char *format, va_list args)
{
    char text[1024];
    va_list copy;
    va_copy(copy, args);
    const int length = std::vsnprintf(text, sizeof(text), format, copy);
    va_end(copy);

    std::lock_guard<std::mutex> lock(s_priorMutex);
    if (length > 0)
    {
        const auto stored = static_cast<std::size_t>(length) < sizeof(text)
            ? static_cast<std::size_t>(length)
            : sizeof(text) - 1;
        s_priorOutput.append(text, stored);
    }
    return length;
}

int callOutput(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = s_outputFunction(format, args);
    va_end(args);
    return result;
}

class CapturingFinalSink final : public rtos::IFinalLogSink
{
public:
    void write(
        const char *text,
        std::size_t length,
        bool truncated) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            lines.emplace_back(text, length);
            truncation.push_back(truncated);
        }
        if (recurse)
        {
            recurse = false;
            callOutput("nested");
        }
    }

    std::mutex mutex;
    std::vector<std::string> lines;
    std::vector<bool> truncation;
    bool recurse = false;
};
}

extern "C" vprintf_like_t esp_log_set_vprintf(vprintf_like_t function)
{
    const auto previous = s_outputFunction;
    s_outputFunction = function;
    return previous;
}

TEST(FinalLogOutputTest, PreservesPriorOutputAndBoundsNonRecursiveSink)
{
    s_outputFunction = priorOutput;
    s_priorOutput.clear();
    CapturingFinalSink sink;
    rtos::setFinalLogSink(&sink);

    EXPECT_EQ(callOutput("value=%d", 7), 7);
    EXPECT_EQ(s_priorOutput, "value=7");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines.front(), "value=7");
    EXPECT_FALSE(sink.truncation.front());

    const std::string exact(255, 'a');
    EXPECT_EQ(callOutput("%s", exact.c_str()), 255);
    ASSERT_EQ(sink.lines.size(), 2u);
    EXPECT_EQ(sink.lines.back(), exact);
    EXPECT_FALSE(sink.truncation.back());

    const std::string oversized(256, 'b');
    EXPECT_EQ(callOutput("%s", oversized.c_str()), 256);
    ASSERT_EQ(sink.lines.size(), 3u);
    EXPECT_EQ(sink.lines.back(), oversized.substr(0, 255));
    EXPECT_TRUE(sink.truncation.back());

    sink.recurse = true;
    EXPECT_EQ(callOutput("outer"), 5);
    EXPECT_EQ(sink.lines.size(), 4u);
    EXPECT_NE(s_priorOutput.find("outernested"), std::string::npos);

    std::vector<std::thread> writers;
    for (int i = 0; i < 8; ++i)
    {
        writers.emplace_back([i] {
            callOutput("thread-%d", i);
        });
    }
    for (auto &writer : writers)
        writer.join();
    EXPECT_EQ(sink.lines.size(), 12u);

    rtos::clearFinalLogSink();
    EXPECT_EQ(callOutput("uart-only"), 9);
    EXPECT_EQ(sink.lines.size(), 12u);
    EXPECT_NE(s_priorOutput.find("uart-only"), std::string::npos);
}
