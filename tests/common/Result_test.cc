/**
 * @file Result_test.cc
 * @brief Result_test.cc 覆盖的单元测试与测试辅助。
 */

#include "Result.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>

namespace
{
using namespace SZX::COMMON;

struct NonDefaultConstructible
{
    explicit NonDefaultConstructible(int input) :
        value(input)
    {
    }

    int value;
};

TEST(common_ResultTest, SuccessStoresValue)
{
    auto result = Result<int>::Success(42);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.error, 0);
    EXPECT_TRUE(result.message.empty());
    EXPECT_EQ(result.value, 42);
}

TEST(common_ResultTest, FailureStoresErrorAndDefaultValue)
{
    auto result = Result<int>::Failure(7, "bad input");

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, 7);
    EXPECT_EQ(result.message, "bad input");
    EXPECT_EQ(result.value, 0);
}

TEST(common_ResultTest, VoidResultHasNoValuePayload)
{
    auto success = Result<void>::Success();
    auto failure = Result<void>::Failure(9, "failed");

    EXPECT_TRUE(success.ok);
    EXPECT_EQ(success.error, 0);
    EXPECT_FALSE(failure.ok);
    EXPECT_EQ(failure.error, 9);
    EXPECT_EQ(failure.message, "failed");
}

TEST(common_ResultTest, SuccessSupportsNonDefaultConstructibleValue)
{
    auto result = Result<NonDefaultConstructible>::Success(NonDefaultConstructible{42});

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.value.value, 42);
    static_assert(!Result<NonDefaultConstructible>::canCreateFailureWithoutValue);
}

TEST(common_ResultTest, FailureSupportsMoveOnlyDefaultConstructibleValue)
{
    auto failure = Result<std::unique_ptr<int>>::Failure(3, "missing");

    EXPECT_FALSE(failure.ok);
    EXPECT_EQ(failure.error, 3);
    EXPECT_EQ(failure.message, "missing");
    EXPECT_EQ(failure.value, nullptr);
    static_assert(Result<std::unique_ptr<int>>::canCreateFailureWithoutValue);
}
} // namespace
