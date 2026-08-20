#include <gtest/gtest.h>

#include "Result.hpp"

#include <memory>

using namespace SZX::COMMON;

namespace
{
struct NonDefaultConstructible
{
    explicit NonDefaultConstructible(int input) : value(input) {}
    int value;
};
} // namespace

TEST(CommonResultTest, StoresSuccessAndFailure)
{
    const auto success = Result<int>::Success(42);
    EXPECT_TRUE(success.ok);
    EXPECT_EQ(success.value, 42);

    const auto failure = Result<int>::Failure(7, "bad input");
    EXPECT_FALSE(failure.ok);
    EXPECT_EQ(failure.error, 7);
    EXPECT_EQ(failure.message, "bad input");
    EXPECT_EQ(failure.value, 0);
}

TEST(CommonResultTest, SupportsVoidAndMoveOnlyValues)
{
    EXPECT_TRUE(Result<void>::Success().ok);
    const auto voidFailure = Result<void>::Failure(9, "failed");
    EXPECT_FALSE(voidFailure.ok);
    EXPECT_EQ(voidFailure.error, 9);

    const auto value = Result<NonDefaultConstructible>::Success(NonDefaultConstructible{42});
    EXPECT_EQ(value.value.value, 42);
    const auto moveOnlyFailure = Result<std::unique_ptr<int>>::Failure(3, "missing");
    EXPECT_FALSE(moveOnlyFailure.value);
}
