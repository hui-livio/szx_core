/**
 * @file Defer_test.cc
 * @brief Defer_test.cc 覆盖的单元测试与测试辅助。
 */

#include "Defer.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace
{
using namespace SZX::COMMON;

TEST(common_DeferTest, ExecutesAtScopeExit)
{
    int value = 0;
    {
        defer
        {
            value = 5;
        };
        EXPECT_EQ(value, 0);
    }

    EXPECT_EQ(value, 5);
}

TEST(common_DeferTest, CancelSkipsCallback)
{
    int value = 0;
    {
        auto guard = Defer([&] { value = 1; });
        guard.Cancel();
    }

    EXPECT_EQ(value, 0);
}

TEST(common_DeferTest, MoveTransfersOwnership)
{
    int callCount = 0;
    {
        auto first  = Defer([&] { ++callCount; });
        auto second = std::move(first);
        (void)second;
    }

    EXPECT_EQ(callCount, 1);
}

TEST(common_DeferTest, DestructorSuppressesCallbackException)
{
    testing::internal::CaptureStderr();

    EXPECT_NO_THROW({
        auto guard = Defer([] { throw std::runtime_error("close failed"); });
        (void)guard;
    });

    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Defer cleanup failed: close failed"), std::string::npos);
}

TEST(common_DeferTest, DestructorReportsUnknownCallbackException)
{
    testing::internal::CaptureStderr();

    EXPECT_NO_THROW({
        auto guard = Defer([] { throw 7; });
        (void)guard;
    });

    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Defer cleanup failed: unknown exception"), std::string::npos);
}
} // namespace
