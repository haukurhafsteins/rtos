#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "esp_log.h"
#include "rtos/LogSinks.hpp"

// EspIdfLogSink compiled on the host against tests/stubs_log_espidf/esp_log.h. What this
// checks: the sink hands ESP-IDF the bare body (so the native prefix is added once), and it
// respects the compile-time cap the way ESP_LOGx does. What it cannot check is the bytes the
// real formatter prints; that is the boot-log check on a meter (HMO-86).

namespace
{
struct Capture
{
    esp_log_level_t level;
    std::string tag;
    std::string text;
};

std::vector<Capture> s_captures;
}

void esp_log_stub_capture(esp_log_level_t level, const char *tag, const char *format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    s_captures.push_back({level, tag ? tag : "", text});
}

namespace
{
class EspIdfLogSinkTest : public ::testing::Test
{
protected:
    void SetUp() override { s_captures.clear(); }

    static rtos::LogRecord record(rtos::LogLevel level, const char *body, const char *line)
    {
        return rtos::LogRecord{level, "RmtReceiver", body, std::strlen(body), line, std::strlen(line)};
    }
};
}

TEST_F(EspIdfLogSinkTest, WritesTheBareBodyOnceNotTheRtosLine)
{
    rtos::EspIdfLogSink sink;
    sink.writeRecord(record(rtos::LogLevel::Warn, "Unexpected RMT levels: 1 -> 1",
                            "[6621] W/RmtReceiver: Unexpected RMT levels: 1 -> 1"));

    ASSERT_EQ(s_captures.size(), 1u);
    EXPECT_EQ(s_captures[0].level, ESP_LOG_WARN);
    EXPECT_EQ(s_captures[0].tag, "RmtReceiver");
    EXPECT_EQ(s_captures[0].text, "Unexpected RMT levels: 1 -> 1");
}

TEST_F(EspIdfLogSinkTest, MapsEveryLevelToItsEspIdfLevel)
{
    rtos::EspIdfLogSink sink;
    sink.writeRecord(record(rtos::LogLevel::Error, "e", "E/RmtReceiver: e"));
    sink.writeRecord(record(rtos::LogLevel::Warn, "w", "W/RmtReceiver: w"));
    sink.writeRecord(record(rtos::LogLevel::Info, "i", "I/RmtReceiver: i"));

    ASSERT_EQ(s_captures.size(), 3u);
    EXPECT_EQ(s_captures[0].level, ESP_LOG_ERROR);
    EXPECT_EQ(s_captures[1].level, ESP_LOG_WARN);
    EXPECT_EQ(s_captures[2].level, ESP_LOG_INFO);
}

TEST_F(EspIdfLogSinkTest, LevelsAboveTheCompileTimeMaximumAreDroppedLikeEspLogd)
{
    static_assert(LOG_LOCAL_LEVEL == ESP_LOG_INFO, "the stub models the meter's Info cap");
    rtos::EspIdfLogSink sink;
    sink.writeRecord(record(rtos::LogLevel::Debug, "d", "D/RmtReceiver: d"));
    sink.writeRecord(record(rtos::LogLevel::Verbose, "v", "V/RmtReceiver: v"));
    sink.writeRecord(record(rtos::LogLevel::None, "none", "-/RmtReceiver: none"));

    EXPECT_TRUE(s_captures.empty());
}

TEST_F(EspIdfLogSinkTest, LegacyWritePrintsTheGivenLineOnce)
{
    rtos::EspIdfLogSink sink;
    const char *line = "[6621] W/RmtReceiver: Unexpected RMT levels: 1 -> 1";
    sink.write(rtos::LogLevel::Warn, "RmtReceiver", line, std::strlen(line));

    ASSERT_EQ(s_captures.size(), 1u);
    EXPECT_EQ(s_captures[0].text, line);
}
