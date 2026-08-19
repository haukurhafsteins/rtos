#include <csetjmp>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "rtos/FinalLogOutput.hpp"
#include "rtos/Log.hpp"
#include "rtos/LogSinks.hpp"
#include "rtos/assert.hpp"
#include "rtos/time.hpp"
#include "zephyr/kernel.h"
#include "zephyr/sys/printk-hooks.h"

namespace
{
std::int64_t s_ticks = 0;
std::int64_t s_lastSleepMilliseconds = -1;
std::int32_t s_lastSleepMicroseconds = -1;
printk_hook_fn_t s_printkHook = nullptr;
std::string s_consoleOutput;
std::jmp_buf s_panicJump;
bool s_expectPanic = false;

int consoleHook(int character)
{
    s_consoleOutput.push_back(static_cast<char>(character));
    return character;
}

class CapturingFinalSink final : public rtos::IFinalLogSink
{
public:
    void write(const char *text, std::size_t length, bool wasTruncated) noexcept override
    {
        output.append(text, length);
        truncated = truncated || wasTruncated;
    }

    std::string output;
    bool truncated = false;
};

class ZephyrFoundationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        s_ticks = 0;
        s_lastSleepMilliseconds = -1;
        s_lastSleepMicroseconds = -1;
        s_consoleOutput.clear();
        if (!s_printkHook)
            s_printkHook = consoleHook;
        s_expectPanic = false;
        rtos::Log::clearSinks();
        rtos::Log::setGlobalLevel(rtos::LogLevel::Info);
        rtos::clearFinalLogSink();
    }
};
}

extern "C" std::int64_t k_uptime_ticks()
{
    return s_ticks;
}

extern "C" std::uint64_t k_ticks_to_us_floor64(std::int64_t ticks)
{
    return static_cast<std::uint64_t>(ticks * 125);
}

extern "C" int k_sleep(k_timeout_t timeout)
{
    s_lastSleepMilliseconds = timeout.milliseconds;
    return 0;
}

extern "C" int k_usleep(std::int32_t microseconds)
{
    s_lastSleepMicroseconds = microseconds;
    s_ticks += (microseconds + 124) / 125;
    return 0;
}

extern "C" int k_mutex_lock(k_mutex *, k_timeout_t)
{
    return 0;
}

extern "C" int k_mutex_unlock(k_mutex *)
{
    return 0;
}

extern "C" [[noreturn]] void k_panic()
{
    if (s_expectPanic)
        std::longjmp(s_panicJump, 1);
    std::abort();
}

extern "C" void __printk_hook_install(printk_hook_fn_t hook)
{
    s_printkHook = hook;
}

extern "C" printk_hook_fn_t __printk_get_hook()
{
    return s_printkHook;
}

extern "C" int printk(const char *format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (result < 0)
        return result;

    const auto length = result < static_cast<int>(sizeof(text))
        ? static_cast<std::size_t>(result)
        : sizeof(text) - 1;
    for (std::size_t index = 0; index < length; ++index)
    {
        if (s_printkHook)
            s_printkHook(text[index]);
    }
    return result;
}

TEST_F(ZephyrFoundationTest, UsesOneMonotonicMicrosecondBase)
{
    s_ticks = 80;

    EXPECT_EQ(rtos::time::now_us().count(), 10000);
    EXPECT_EQ(rtos::time::now_ms().count(), 10);
    EXPECT_EQ(rtos::time::now_s().count(), 0);
    EXPECT_EQ(rtos::time::HighResClock::now().time_since_epoch().count(), 10000);
}

TEST_F(ZephyrFoundationTest, SleepsWithZephyrMillisecondAndMicrosecondCalls)
{
    rtos::time::sleep_for(rtos::Millis(27));
    EXPECT_EQ(s_lastSleepMilliseconds, 27);

    s_ticks = 80;
    rtos::time::sleep_until(rtos::time::HighResClock::time_point(rtos::Micros(10400)));
    EXPECT_EQ(s_lastSleepMicroseconds, 400);

    s_lastSleepMicroseconds = -1;
    rtos::time::sleep_until(rtos::time::HighResClock::time_point(rtos::Micros(9999)));
    EXPECT_EQ(s_lastSleepMicroseconds, -1);
}

TEST_F(ZephyrFoundationTest, KeepsPrintkSinkSelectionApplicationOwned)
{
    rtos::Log::log(rtos::LogLevel::Info, "test", "unregistered");
    EXPECT_TRUE(s_consoleOutput.empty());

    rtos::ZephyrPrintkSink printkSink;
    rtos::Log::addSink(printkSink);
    rtos::Log::log(rtos::LogLevel::Info, "test", "value=%d", 7);

    EXPECT_EQ(s_consoleOutput, "[0] I/test: value=7\n");
}

TEST_F(ZephyrFoundationTest, FinalOutputRegistrationPreservesThePrintkHook)
{
    CapturingFinalSink sink;
    rtos::setFinalLogSink(&sink);

    printk("hello %d", 7);
    EXPECT_EQ(s_consoleOutput, "hello 7");
    EXPECT_EQ(sink.output, "hello 7");
    EXPECT_FALSE(sink.truncated);

    rtos::clearFinalLogSink();
    printk(" only-console");
    EXPECT_EQ(s_consoleOutput, "hello 7 only-console");
    EXPECT_EQ(sink.output, "hello 7");
}

TEST_F(ZephyrFoundationTest, AssertLogsThenPanics)
{
    rtos::ZephyrPrintkSink printkSink;
    rtos::Log::addSink(printkSink);

    if (setjmp(s_panicJump) == 0)
    {
        s_expectPanic = true;
        rtos::backend::assert_fail("value != 0", "sample.cpp", 42, "run");
    }
    s_expectPanic = false;

    EXPECT_EQ(
        s_consoleOutput,
        "[0] E/rtos: assertion failed: value != 0 at sample.cpp:42 (run)\n");
}
