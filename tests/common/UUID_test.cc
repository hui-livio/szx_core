#define SZX_COMMON_TESTING

#include <gtest/gtest.h>

#include "UUID.hpp"

#include <algorithm>
#include <cstdint>

using namespace SZX::COMMON;

namespace
{
bool HasRfcVariant(const Uuid &uuid)
{
    return (uuid.data[8] & 0xC0) == 0x80;
}
} // namespace

TEST(CommonUuidTest, NilAndParseRoundTrip)
{
    const auto nil = Uuid::nil();
    EXPECT_TRUE(nil.isNil());
    EXPECT_EQ(nil.toString(), "00000000-0000-0000-0000-000000000000");
    const auto parsed = Uuid::parse("{00000000-0000-0000-0000-000000000000}");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, nil);
    EXPECT_FALSE(Uuid::parse("invalid").has_value());
}

TEST(CommonUuidTest, V4AndV7HaveExpectedVersionAndVariant)
{
    const auto v4 = Uuid::v4();
    const auto v7 = Uuid::v7();
    EXPECT_EQ(v4.data[6] >> 4, 4);
    EXPECT_EQ(v7.data[6] >> 4, 7);
    EXPECT_TRUE(HasRfcVariant(v4));
    EXPECT_TRUE(HasRfcVariant(v7));
    const auto parsedV4 = Uuid::parse(v4.toString());
    const auto parsedV7 = Uuid::parse(v7.toString());
    ASSERT_TRUE(parsedV4.has_value());
    ASSERT_TRUE(parsedV7.has_value());
    EXPECT_EQ(*parsedV4, v4);
    EXPECT_EQ(*parsedV7, v7);
}

TEST(CommonUuidTest, V7ClockHandlesCounterOverflowAndRollback)
{
    UuidV7Clock::ResetForTesting(1000, 0xFFFE);
    EXPECT_EQ(UuidV7Clock::next(1000).counter, 0xFFFF);
    const auto overflowed = UuidV7Clock::next(1000);
    EXPECT_EQ(overflowed.ms, 1001U);
    EXPECT_EQ(overflowed.counter, 0U);

    UuidV7Clock::ResetForTesting(2000, 7);
    const auto rollback = UuidV7Clock::next(1999);
    EXPECT_EQ(rollback.ms, 2000U);
    EXPECT_EQ(rollback.counter, 8U);
    UuidV7Clock::ResetForTesting();
}
