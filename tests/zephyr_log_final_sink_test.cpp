#include "zephyr_log_harness.hpp"
#include <algorithm>
#include <thread>

using namespace zephyr_log_test;

TEST_F(ZephyrLogFinalSinkTest, WholeRecordAndUnchangedConsole) {
    RTOS_LOGI("AHRS", "Set ended reason=%u reps=%u duration_ms=%u", 19u, 2u, 42u);
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0], "[0] I/AHRS: Set ended reason=19 reps=2 duration_ms=42");
    EXPECT_FALSE(sink.truncation[0]);
    EXPECT_EQ(console, sink.lines[0] + "\n");
}
TEST_F(ZephyrLogFinalSinkTest, NormalizesTerminatorsWithoutChangingConsole) {
    RTOS_LOGI("test", "one\r\n");
    RTOS_LOGI("test", "two\ninside\n");
    ASSERT_EQ(sink.lines.size(), 2u);
    EXPECT_EQ(sink.lines[0], "[0] I/test: one");
    EXPECT_EQ(sink.lines[1], "[0] I/test: two\ninside");
    EXPECT_EQ(console, "[0] I/test: one\r\n\n[0] I/test: two\ninside\n\n");
}
TEST_F(ZephyrLogFinalSinkTest, EmptyBodyIsOneFormattedRecordAndEmptyNativeWriteIsNotCaptured) {
    printk("");
    EXPECT_TRUE(sink.lines.empty());
    RTOS_LOGI("test", "%s", "");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0], "[0] I/test: ");
    EXPECT_FALSE(sink.truncation[0]);
}
TEST_F(ZephyrLogFinalSinkTest, ReportsBothFormattingTruncationBoundaries) {
    const std::string prefix = "[0] I/test: ";
    const std::string exact(RTOS_LOG_LINE_MAX - 1 - prefix.size(), 'a');
    RTOS_LOGI("test", "%s", exact.c_str());
    RTOS_LOGI("test", "%sx", exact.c_str());
    const std::string oversized(RTOS_LOG_LINE_MAX + 50, 'b');
    RTOS_LOGI("test", "%s", oversized.c_str());
    ASSERT_EQ(sink.lines.size(), 3u);
    EXPECT_EQ(sink.lines[0].size(), RTOS_LOG_LINE_MAX - 1u);
    EXPECT_FALSE(sink.truncation[0]);
    EXPECT_TRUE(sink.truncation[1]);
    EXPECT_TRUE(sink.truncation[2]);
}
TEST_F(ZephyrLogFinalSinkTest, RecursionAndNativeOutputDoNotDuplicateCapture) {
    sink.recurse = true;
    RTOS_LOGI("test", "outer");
    printk("native\n");
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0], "[0] I/test: outer");
    EXPECT_NE(console.find("nested native\n"), std::string::npos);
    EXPECT_NE(console.find("native\n"), std::string::npos);
    rtos::clearFinalLogSink();
    RTOS_LOGI("test", "after clear");
    EXPECT_EQ(sink.lines.size(), 1u);
    EXPECT_NE(console.find("after clear"), std::string::npos);
}
TEST_F(ZephyrLogFinalSinkTest, NativeIsrOutputPreservesConsoleWithoutCapture) {
    testInIsr = true;
    printk("native ISR\n");
    testInIsr = false;
    EXPECT_TRUE(sink.lines.empty());
    EXPECT_EQ(console, "native ISR\n");
}
TEST_F(ZephyrLogFinalSinkTest, ConcurrentCallersHaveIndependentCompleteRecords) {
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 8; ++i)
        threads.emplace_back([i] { RTOS_LOGI("test", "thread-%u", i); });
    for (auto &thread : threads) thread.join();
    ASSERT_EQ(sink.lines.size(), 8u);
    for (unsigned i = 0; i < 8; ++i) {
        const auto expected = "[0] I/test: thread-" + std::to_string(i);
        EXPECT_EQ(std::count(sink.lines.begin(), sink.lines.end(), expected), 1);
    }
}
