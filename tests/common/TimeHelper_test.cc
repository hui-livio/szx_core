/**
 * @file TimeHelper_test.cc
 * @brief TimeHelper_test.cc 覆盖的单元测试与测试辅助。
 */

#include "TimeHelper.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace
{
using namespace SZX::COMMON;

TEST(common_TimeHelperTest, FormatsAndParsesUtcTimestamp)
{
    constexpr int64_t timestamp = 1'766'665'625'000;

    const auto text = TimeHelper::TimestampToString(timestamp, "%Y-%m-%d %H:%M:%S", true);
    EXPECT_EQ(text, "2025-12-25 12:27:05");

    const auto parsed = TimeHelper::StringToTimestamp(text, "%Y-%m-%d %H:%M:%S", true);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, timestamp);
}

TEST(common_TimeHelperTest, Iso8601RoundTripAndRejectsInvalidShape)
{
    constexpr int64_t timestamp = 1'766'665'625'000;

    const auto iso = TimeHelper::TimestampToIso8601(timestamp);
    EXPECT_EQ(iso, "2025-12-25T12:27:05Z");

    const auto parsed = TimeHelper::Iso8601ToTimestamp(iso);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, timestamp);

    EXPECT_FALSE(TimeHelper::Iso8601ToTimestamp("2025-12-25T12:00:25").has_value());
    EXPECT_FALSE(TimeHelper::Iso8601ToTimestamp("not-a-time").has_value());
}

TEST(common_TimeHelperTest, RejectsInvalidFormattedTimestamp)
{
    EXPECT_FALSE(TimeHelper::StringToTimestamp("not-a-time", "%Y-%m-%d %H:%M:%S", true).has_value());
    EXPECT_FALSE(TimeHelper::StringToTimestamp("2025/12/25 12:27:05", "%Y-%m-%d %H:%M:%S", true).has_value());
    EXPECT_FALSE(TimeHelper::StringToTimestamp("2025-12-25 12:27:05xxx", "%Y-%m-%d %H:%M:%S", true).has_value());
    EXPECT_FALSE(TimeHelper::StringToTimestamp("2025-02-29 12:27:05", "%Y-%m-%d %H:%M:%S", true).has_value());
}

TEST(common_TimeHelperTest, AllowsTrailingWhitespaceAfterFormattedTimestamp)
{
    const auto parsed = TimeHelper::StringToTimestamp("2025-12-25 12:27:05  ", "%Y-%m-%d %H:%M:%S", true);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, 1'766'665'625'000);
}

TEST(common_TimeHelperTest, LocalTimeFormatAndParseRoundTrip)
{
    constexpr int64_t timestamp = 1'766'665'625'000;

    const auto text = TimeHelper::TimestampToString(timestamp, "%Y-%m-%d %H:%M:%S", false);
    ASSERT_FALSE(text.empty());

    const auto parsed = TimeHelper::StringToTimestamp(text, "%Y-%m-%d %H:%M:%S", false);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, timestamp);
}

TEST(common_TimeHelperTest, TimestampToStringReturnsEmptyForOversizedFormatOutput)
{
    const auto text = TimeHelper::TimestampToString(
        1'766'665'625'000, "%Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M:%S %Y-%m-%d %H:%M:%S", true);

    EXPECT_TRUE(text.empty());
}

TEST(common_TimeHelperTest, NowTimestampIsCloseToSystemClock)
{
    const auto before =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto now = TimeHelper::NowTimestamp();
    const auto after =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    EXPECT_GE(now, before);
    EXPECT_LE(now, after);
}

TEST(common_TimeHelperTest, ScopeTimerInvokesCallbackOnlyOnce)
{
    int callCount   = 0;
    int64_t elapsed = -1;

    {
        ScopeTimer timer(
            [&](int64_t ms)
            {
                ++callCount;
                elapsed = ms;
            });
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        timer.Stop();
        timer.Stop();
    }

    EXPECT_EQ(callCount, 1);
    EXPECT_GE(elapsed, 0);
}

TEST(common_TimeHelperTest, StopwatchResetRestartsElapsedTime)
{
    Stopwatch stopwatch;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto beforeReset = stopwatch.ElapsedMs();

    stopwatch.Reset();
    const auto afterReset = stopwatch.ElapsedMs();

    EXPECT_GE(beforeReset, 0);
    EXPECT_GE(afterReset, 0);
    EXPECT_LE(afterReset, beforeReset);
}

TEST(common_TimeHelperTest, ScopeTimerMoveConstructorTransfersCallback)
{
    int callCount = 0;

    {
        ScopeTimer first([&](int64_t) { ++callCount; });
        ScopeTimer second(std::move(first));
    }

    EXPECT_EQ(callCount, 1);
}

TEST(common_TimeHelperTest, ScopeTimerMoveAssignmentStopsPreviousTimerAndTransfersCallback)
{
    int firstCallCount  = 0;
    int secondCallCount = 0;

    {
        ScopeTimer first([&](int64_t) { ++firstCallCount; });
        ScopeTimer second([&](int64_t) { ++secondCallCount; });

        second = std::move(first);
        EXPECT_EQ(secondCallCount, 1);
    }

    EXPECT_EQ(firstCallCount, 1);
    EXPECT_EQ(secondCallCount, 1);
}
} // namespace
