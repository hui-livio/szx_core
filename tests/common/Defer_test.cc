#include <gtest/gtest.h>

#include "Defer.hpp"

#include <stdexcept>

using namespace SZX::COMMON;

TEST(CommonDeferTest, ExecutesAndCanBeCancelled)
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

    {
        auto guard = Defer([&] { value = 9; });
        guard.Cancel();
    }
    EXPECT_EQ(value, 5);
}

TEST(CommonDeferTest, MoveTransfersOwnershipAndSuppressesExceptions)
{
    int calls = 0;
    {
        auto first  = Defer([&] { ++calls; });
        auto second = std::move(first);
        (void)second;
    }
    EXPECT_EQ(calls, 1);

    EXPECT_NO_THROW({
        auto guard = Defer([] { throw std::runtime_error("cleanup"); });
        (void)guard;
    });
}
