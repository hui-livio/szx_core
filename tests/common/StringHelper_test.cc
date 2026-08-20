#include <gtest/gtest.h>

#include "StringHelper.hpp"

#include <string_view>
#include <vector>

using namespace SZX::COMMON;

TEST(CommonStringHelperTest, StripsAndChangesAsciiCase)
{
    EXPECT_EQ(StringHelper::Strip(" \t hello\r\n"), "hello");
    EXPECT_EQ(StringHelper::LStrip(" \t hello "), "hello ");
    EXPECT_EQ(StringHelper::RStrip(" hello \n"), " hello");
    EXPECT_EQ(StringHelper::ToLower("AbC-123"), "abc-123");
    EXPECT_EQ(StringHelper::ToUpper("AbC-123"), "ABC-123");
}

TEST(CommonStringHelperTest, ParsesIntegersStrictly)
{
    EXPECT_EQ(StringHelper::ParseInteger<int>("42"), 42);
    EXPECT_EQ(StringHelper::ParseInteger<unsigned int>("ff", 16), 255U);
    EXPECT_FALSE(StringHelper::ParseInteger<int>(" 42").has_value());
    EXPECT_FALSE(StringHelper::ParseInteger<int>("42x").has_value());
}

TEST(CommonStringHelperTest, SplitsAndJoinsWithEmptyPartPolicy)
{
    EXPECT_EQ(StringHelper::Split("a,,b,", ',', false), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(StringHelper::Split("left--right", "--"), (std::vector<std::string>{"left", "right"}));
    EXPECT_EQ(StringHelper::Join(std::vector<std::string>{"a", "b", "c"}, ","), "a,b,c");
    EXPECT_EQ(StringHelper::Join(std::vector<std::string_view>{"left", "right"}, " -> "), "left -> right");
}
