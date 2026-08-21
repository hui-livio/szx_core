/**
 * @file StringHelper_test.cc
 * @brief StringHelper_test.cc 覆盖的单元测试与测试辅助。
 */

#include "StringHelper.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace
{
using namespace SZX::COMMON;


TEST(common_StringHelperTest, StripRemovesAsciiWhitespaceFromBothSides)
{
    EXPECT_EQ(StringHelper::Strip(" \t hello\r\n"), "hello");
    EXPECT_EQ(StringHelper::LStrip(" \t hello "), "hello ");
    EXPECT_EQ(StringHelper::RStrip(" hello \n"), " hello");
    EXPECT_EQ(StringHelper::Strip(" \t\r\n"), "");
}

TEST(common_StringHelperTest, ConvertsAsciiCase)
{
    EXPECT_EQ(StringHelper::ToLower("AbC-123"), "abc-123");
    EXPECT_EQ(StringHelper::ToUpper("AbC-123"), "ABC-123");
}

TEST(common_StringHelperTest, StrictlyParsesIntegers)
{
    EXPECT_EQ(StringHelper::ParseInteger<int>("42"), 42);
    EXPECT_EQ(StringHelper::ParseInteger<int>("-42"), -42);
    EXPECT_EQ(StringHelper::ParseInteger<unsigned int>("ff", 16), 255U);
    EXPECT_FALSE(StringHelper::ParseInteger<int>("").has_value());
    EXPECT_FALSE(StringHelper::ParseInteger<int>(" 42").has_value());
    EXPECT_FALSE(StringHelper::ParseInteger<int>("42x").has_value());
    EXPECT_FALSE(StringHelper::ParseInteger<int>("999999999999999999999999").has_value());
}

TEST(common_StringHelperTest, SplitByCharCanKeepOrDropEmptyParts)
{
    const auto keepEmpty = StringHelper::Split("a,,b,", ',');
    ASSERT_EQ(keepEmpty.size(), 4);
    EXPECT_EQ(keepEmpty[0], "a");
    EXPECT_EQ(keepEmpty[1], "");
    EXPECT_EQ(keepEmpty[2], "b");
    EXPECT_EQ(keepEmpty[3], "");

    const auto dropEmpty = StringHelper::Split("a,,b,", ',', false);
    ASSERT_EQ(dropEmpty.size(), 2);
    EXPECT_EQ(dropEmpty[0], "a");
    EXPECT_EQ(dropEmpty[1], "b");
}

TEST(common_StringHelperTest, SplitByStringDelimiter)
{
    const auto parts = StringHelper::Split("left--middle--right", "--");
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "left");
    EXPECT_EQ(parts[1], "middle");
    EXPECT_EQ(parts[2], "right");

    const auto noDelimiter = StringHelper::Split("abc", "");
    ASSERT_EQ(noDelimiter.size(), 1);
    EXPECT_EQ(noDelimiter[0], "abc");
}

TEST(common_StringHelperTest, JoinStringsAndStringViews)
{
    EXPECT_EQ(StringHelper::Join(std::vector<std::string>{"a", "b", "c"}, ","), "a,b,c");

    const auto parts = std::vector<std::string_view>{"left", "right"};
    EXPECT_EQ(StringHelper::Join(parts, " -> "), "left -> right");
}

} // namespace
