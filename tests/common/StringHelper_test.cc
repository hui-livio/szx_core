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

TEST(common_StringHelperTest, ConvertsUtf8AndWideTextWithoutLocale)
{
    const std::string utf8  = "A\xE4\xB8\xAD\xF0\x9F\x98\x80";
    const std::wstring wide = L"A\u4e2d\U0001f600";
    EXPECT_EQ(StringHelper::fromutf8(utf8), wide);
    EXPECT_EQ(StringHelper::toutf8(wide), utf8);
    EXPECT_TRUE(StringHelper::fromutf8(std::string_view{}).empty());
    EXPECT_TRUE(StringHelper::toutf8(std::wstring_view{}).empty());
}

TEST(common_StringHelperTest, PreservesNullBomAndUnicodeEncodingBoundaries)
{
    const std::string utf8  = "\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80"
                              "\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf";
    const std::wstring wide = L"\x7f\u0080\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff";
    EXPECT_EQ(StringHelper::fromutf8(utf8), wide);
    EXPECT_EQ(StringHelper::toutf8(wide), utf8);
    const std::string embedded{"A\0\xef\xbb\xbfZ", 6};
    const std::wstring expected{L"A\0\ufeffZ", 4};
    EXPECT_EQ(StringHelper::fromutf8(embedded), expected);
    EXPECT_EQ(StringHelper::toutf8(expected), embedded);
}

TEST(common_StringHelperTest, RejectsInvalidUtf8AndWideSurrogates)
{
    for (const auto &invalid :
         {std::string{"\x80"}, std::string{"\xC0\x80"}, std::string{"\xE0\x80\x80"}, std::string{"\xF4\x90\x80\x80"},
          std::string{"\xF0\x9F\x98"}, std::string{"\xc2"}, std::string{"\xe0\xa0"}, std::string{"\xed\xa0\x80"},
          std::string{"\xed\xbf\xbf"}, std::string{"\xf0\x80\x80\x80"}, std::string{"\xff"}, std::string{"\xc2Z"},
          std::string{"\xe1\x80Z"}})
        EXPECT_THROW(StringHelper::fromutf8(invalid), std::invalid_argument);
    EXPECT_THROW(StringHelper::toutf8(std::wstring(1, static_cast<wchar_t>(0xd800))), std::invalid_argument);
    EXPECT_THROW(StringHelper::toutf8(std::wstring(1, static_cast<wchar_t>(0xdc00))), std::invalid_argument);
    if constexpr (sizeof(wchar_t) == 2)
    {
        const std::wstring invalidPair{static_cast<wchar_t>(0xd800), L'A'};
        EXPECT_THROW(StringHelper::toutf8(invalidPair), std::invalid_argument);
    }
    else
    {
        EXPECT_THROW(StringHelper::toutf8(std::wstring(1, static_cast<wchar_t>(0x110000))), std::invalid_argument);
        EXPECT_THROW(StringHelper::toutf8(std::wstring(1, static_cast<wchar_t>(-1))), std::invalid_argument);
    }
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
