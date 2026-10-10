#pragma once
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk-hooks.h>
#include <zephyr/sys/printk.h>
#include "rtos/FinalLogOutput.hpp"
#include "rtos/Log.hpp"
#include "rtos/LogSinks.hpp"

namespace zephyr_log_test {
inline std::mutex consoleMutex;
inline std::string console;
inline int consoleChar(int character) {
    std::lock_guard guard(consoleMutex);
    console += static_cast<char>(character);
    return character;
}
inline printk_hook_fn_t hook = consoleChar;
class FinalSink final : public rtos::IFinalLogSink {
public:
    std::mutex mutex;
    std::vector<std::string> lines;
    std::vector<bool> truncation;
    bool recurse = false;
    void write(const char *text, std::size_t length, bool truncated) noexcept override {
        {
            std::lock_guard guard(mutex);
            lines.emplace_back(text, length);
            truncation.push_back(truncated);
        }
        if (recurse) {
            recurse = false;
            RTOS_LOGI("test", "nested RTOS");
            printk("nested native\n");
        }
    }
};
class ZephyrLogFinalSinkTest : public ::testing::Test {
protected:
    FinalSink sink;
    rtos::ZephyrPrintkSink printer;
    void SetUp() override {
        console.clear();
        rtos::Log::clearSinks();
        rtos::Log::setGlobalLevel(rtos::LogLevel::Info);
        rtos::Log::setTimestampProvider(nullptr);
        rtos::Log::addSink(printer);
        rtos::setFinalLogSink(&sink);
    }
    void TearDown() override {
        rtos::clearFinalLogSink();
        rtos::Log::clearSinks();
        testInIsr = false;
    }
};
}

extern "C" void __printk_hook_install(printk_hook_fn_t hook) {
    zephyr_log_test::hook = hook;
}
extern "C" printk_hook_fn_t __printk_get_hook() {
    return zephyr_log_test::hook;
}
extern "C" int printk(const char *format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    // NCS vprintk routes directly into Zephyr logging with LOG_PRINTK=y.
    // Otherwise cbvprintf invokes the installed character hook.
    for (const char *p = text; *p; ++p) {
#if CONFIG_LOG_PRINTK
        zephyr_log_test::consoleChar(*p);
#else
        zephyr_log_test::hook(*p);
#endif
    }
    return result;
}
