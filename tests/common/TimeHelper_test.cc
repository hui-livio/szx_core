#include <gtest/gtest.h>

#include "TimeHelper.hpp"

#include <chrono>

using namespace SZX::COMMON;

TEST(CommonTimeHelperTest, FormatsAndParsesUtcAndIso8601)
{
    constexpr std::int64_t timestamp = 1'766'665'625'000;
    EXPECT_EQ(TimeHelper::TimestampToString(timestamp, "%Y-%m-%d %H:%M:%S", true), "2025-12-25 12:27:05");
    EXPECT_EQ(TimeHelper::TimestampToIso8601(timestamp), "2025-12-25T12:27:05Z");
    EXPECT_EQ(TimeHelper::Iso8601ToTimestamp("2025-12-25T12:27:05Z"), timestamp);
}

TEST(CommonTimeHelperTest, RejectsInvalidValuesAndAllowsTrailingWhitespace)
{
    EXPECT_FALSE(TimeHelper::StringToTimestamp("not-a-time", "%Y-%m-%d %H:%M:%S", true).has_value());
    EXPECT_FALSE(TimeHelper::Iso8601ToTimestamp("2025-12-25T12:00:25").has_value());
    const auto parsed = TimeHelper::StringToTimestamp("2025-12-25 12:27:05  ", "%Y-%m-%d %H:%M:%S", true);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, 1'766'665'625'000);
}

TEST(CommonTimeHelperTest, ScopeTimerStopsOnceAndStopwatchResets)
{
    int calls = 0;
    {
        ScopeTimer timer(
            [&](std::int64_t elapsed)
            {
                ++calls;
                EXPECT_GE(elapsed, 0);
            });
        timer.Stop();
        timer.Stop();
    }
    EXPECT_EQ(calls, 1);

    Stopwatch stopwatch;
    const auto beforeReset = stopwatch.ElapsedMs();
    stopwatch.Reset();
    EXPECT_GE(beforeReset, 0);
    EXPECT_GE(stopwatch.ElapsedMs(), 0);
}
