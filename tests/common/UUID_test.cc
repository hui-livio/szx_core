/**
 * @file UUID_test.cc
 * @brief UUID_test.cc 覆盖的单元测试与测试辅助。
 */

#define SZX_COMMON_TESTING
#include "UUID.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{
using namespace SZX::COMMON;

bool HasRfcVariant(const Uuid &uuid)
{
    return (uuid.data[8] & 0xC0) == 0x80;
}

uint64_t V7TimestampMs(const Uuid &uuid)
{
    return (static_cast<uint64_t>(uuid.data[0]) << 40) | (static_cast<uint64_t>(uuid.data[1]) << 32) |
        (static_cast<uint64_t>(uuid.data[2]) << 24) | (static_cast<uint64_t>(uuid.data[3]) << 16) |
        (static_cast<uint64_t>(uuid.data[4]) << 8) | static_cast<uint64_t>(uuid.data[5]);
}

TEST(common_UUIDTest, NilAndParseRoundTrip)
{
    const auto nil = Uuid::nil();

    EXPECT_TRUE(nil.isNil());
    EXPECT_EQ(nil.toString(), "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(nil.toStringCompact(), "00000000000000000000000000000000");

    const auto parsed = Uuid::parse("{00000000-0000-0000-0000-000000000000}");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, nil);
}

TEST(common_UUIDTest, RejectsInvalidText)
{
    EXPECT_FALSE(Uuid::parse("").has_value());
    EXPECT_FALSE(Uuid::parse("00000000_0000_0000_0000_000000000000").has_value());
    EXPECT_FALSE(Uuid::parse("zzzzzzzz-0000-0000-0000-000000000000").has_value());
    EXPECT_FALSE(Uuid::parse("00000000-0000-0000-0000-00000000000").has_value());
}

TEST(common_UUIDTest, V4HasExpectedVersionAndVariant)
{
    const auto uuid = Uuid::v4();

    EXPECT_EQ((uuid.data[6] >> 4), 4);
    EXPECT_TRUE(HasRfcVariant(uuid));

    const auto parsed = Uuid::parse(uuid.toString());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, uuid);
}

TEST(common_UUIDTest, SupportsStringConcatenation)
{
    const auto uuid = Uuid::nil();
    const auto text = uuid.toString();

    EXPECT_EQ(uuid + "-suffix", text + "-suffix");
    EXPECT_EQ("prefix-" + uuid, "prefix-" + text);
    EXPECT_EQ(uuid + std::string("-tail"), text + "-tail");
    EXPECT_EQ(std::string("head-") + uuid, "head-" + text);

    std::string output  = "id=";
    output             += uuid;
    EXPECT_EQ(output, "id=" + text);
}

TEST(common_UUIDTest, V7IsMonotonicWithinSingleThread)
{
    std::vector<Uuid> values;
    values.reserve(128);

    for (int i = 0; i < 128; ++i)
    {
        values.push_back(Uuid::v7());
        EXPECT_EQ((values.back().data[6] >> 4), 7);
        EXPECT_TRUE(HasRfcVariant(values.back()));
    }

    EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
}

TEST(common_UUIDTest, V7ClockAdvancesTimestampWhenCounterOverflows)
{
    UuidV7Clock::ResetForTesting(1000, 0xFFFE);

    const auto lastInSameMs = UuidV7Clock::next(1000);
    EXPECT_EQ(lastInSameMs.ms, 1000U);
    EXPECT_EQ(lastInSameMs.counter, 0xFFFF);

    const auto overflowed = UuidV7Clock::next(1000);
    EXPECT_EQ(overflowed.ms, 1001U);
    EXPECT_EQ(overflowed.counter, 0U);

    UuidV7Clock::ResetForTesting();
}

TEST(common_UUIDTest, V7ClockKeepsMonotonicTimestampWhenObservedClockRollsBack)
{
    UuidV7Clock::ResetForTesting(2000, 7);

    const auto tick = UuidV7Clock::next(1999);
    EXPECT_EQ(tick.ms, 2000U);
    EXPECT_EQ(tick.counter, 8U);

    UuidV7Clock::ResetForTesting();
}

TEST(common_UUIDTest, V7EncodesMonotonicClockTimestamp)
{
    UuidV7Clock::ResetForTesting();

    const auto uuid = Uuid::v7();

    EXPECT_EQ((uuid.data[6] >> 4), 7);
    EXPECT_TRUE(HasRfcVariant(uuid));
    EXPECT_LE(V7TimestampMs(uuid), static_cast<std::uint64_t>(TimeHelper::NowTimestamp()));
}

TEST(common_UUIDTest, ConcurrentGenerationProducesValidUniqueStrings)
{
    constexpr int kThreads   = 8;
    constexpr int kPerThread = 256;

    std::mutex mutex;
    std::unordered_set<std::string> generated;
    std::vector<std::thread> threads;
    std::atomic<bool> allGeneratedValuesValid{true};

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&]
            {
                std::vector<std::string> local;
                local.reserve(kPerThread * 2);
                for (int i = 0; i < kPerThread; ++i)
                {
                    const auto v4       = Uuid::v4();
                    const auto v7       = Uuid::v7();
                    const auto parsedV4 = Uuid::parse(v4.toString());
                    const auto parsedV7 = Uuid::parse(v7.toString());

                    if ((v4.data[6] >> 4) != 4 || !HasRfcVariant(v4) || !parsedV4.has_value() || *parsedV4 != v4 ||
                        (v7.data[6] >> 4) != 7 || !HasRfcVariant(v7) || !parsedV7.has_value() || *parsedV7 != v7)
                    {
                        allGeneratedValuesValid.store(false, std::memory_order_relaxed);
                    }

                    local.push_back(v4.toString());
                    local.push_back(v7.toString());
                }

                std::lock_guard<std::mutex> lock(mutex);
                generated.insert(local.begin(), local.end());
            });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_TRUE(allGeneratedValuesValid.load(std::memory_order_relaxed));
    EXPECT_EQ(generated.size(), static_cast<size_t>(kThreads * kPerThread * 2));
}
} // namespace
