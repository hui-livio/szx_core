/**
 * @file JsonHelper_test.cc
 * @brief JsonHelper_test.cc 覆盖的单元测试与测试辅助。
 */

#include "JsonHelper.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <string>

namespace
{
using namespace SZX::COMMON;

TEST(common_JsonHelperTest, SafelyParsesAnyJsonValue)
{
    const auto object = JsonHelper::Parse(R"({"a":1})");
    ASSERT_TRUE(object.has_value());
    EXPECT_EQ((*object)["a"], 1);

    const auto array = JsonHelper::Parse("[1,2]");
    ASSERT_TRUE(array.has_value());
    EXPECT_TRUE(array->is_array());
    EXPECT_FALSE(JsonHelper::Parse("").has_value());
    EXPECT_FALSE(JsonHelper::Parse("not-json").has_value());
}


TEST(common_JsonHelperTest, ParseObjectReturnsEmptyObjectForInvalidInput)
{
    EXPECT_TRUE(JsonHelper::ParseObject(std::optional<std::string>{}).empty());
    EXPECT_TRUE(JsonHelper::ParseObject("[]").empty());
    EXPECT_TRUE(JsonHelper::ParseObject("not-json").empty());

    const auto parsed = JsonHelper::ParseObject(R"({"a":1})");
    ASSERT_TRUE(parsed.is_object());
    EXPECT_EQ(parsed["a"], 1);
}

TEST(common_JsonHelperTest, ParseRequiredObjectRejectsNonObjects)
{
    EXPECT_FALSE(JsonHelper::ParseRequiredObject("[]").has_value());
    EXPECT_FALSE(JsonHelper::ParseRequiredObject("not-json").has_value());

    const auto parsed = JsonHelper::ParseRequiredObject(R"({"a":1})");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ((*parsed)["a"], 1);
}

TEST(common_JsonHelperTest, DeepMergeRecursivelyMergesObjects)
{
    const auto base  = Json::parse(R"({"outer":{"a":1,"b":2},"keep":true})");
    const auto patch = Json::parse(R"({"outer":{"b":3,"c":4},"new":"value"})");

    const auto merged = JsonHelper::DeepMerge(base, patch);
    EXPECT_EQ(merged["outer"]["a"], 1);
    EXPECT_EQ(merged["outer"]["b"], 3);
    EXPECT_EQ(merged["outer"]["c"], 4);
    EXPECT_EQ(merged["keep"], true);
    EXPECT_EQ(merged["new"], "value");
}

TEST(common_JsonHelperTest, ConvertsJsonValuesToStringsAndIntegers)
{
    EXPECT_FALSE(JsonHelper::ToOptionalString(Json::object()).has_value());
    EXPECT_FALSE(JsonHelper::ToOptionalString(nullptr).has_value());
    EXPECT_EQ(JsonHelper::ToOptionalString(Json{{"a", 1}}), R"({"a":1})");

    EXPECT_EQ(JsonHelper::ToString("abc"), "abc");
    EXPECT_EQ(JsonHelper::ToString(nullptr), "");
    EXPECT_EQ(JsonHelper::ToString(Json{{"a", 1}}), R"({"a":1})");

    EXPECT_EQ(JsonHelper::ToInt(12, -1), 12);
    EXPECT_EQ(JsonHelper::ToInt(12.8, -1), 12);
    EXPECT_EQ(JsonHelper::ToInt(" 42 ", -1), 42);
    EXPECT_EQ(JsonHelper::ToInt("42abc", -1), -1);
    EXPECT_EQ(JsonHelper::ToInt("", -1), -1);
    EXPECT_EQ(JsonHelper::ToInt("999999999999999999999999", -1), -1);
    EXPECT_EQ(JsonHelper::ToInt("bad", -1), -1);
}

TEST(common_JsonHelperTest, ConvertsBooleansWithExplicitStrictAndCoercedSemantics)
{
    EXPECT_EQ(JsonHelper::ToOptionalBoolStrict(true), true);
    EXPECT_FALSE(JsonHelper::ToOptionalBoolStrict(1).has_value());
    EXPECT_FALSE(JsonHelper::ToOptionalBoolStrict("true").has_value());

    EXPECT_EQ(JsonHelper::ToOptionalBoolCoerced(false), false);
    EXPECT_EQ(JsonHelper::ToOptionalBoolCoerced(1), true);
    EXPECT_EQ(JsonHelper::ToOptionalBoolCoerced(0.0), false);
    EXPECT_EQ(JsonHelper::ToOptionalBoolCoerced(" Enabled "), true);
    EXPECT_EQ(JsonHelper::ToOptionalBoolCoerced("off"), false);
    EXPECT_FALSE(JsonHelper::ToOptionalBoolCoerced("unknown").has_value());
    EXPECT_FALSE(JsonHelper::ToOptionalBoolCoerced(Json::array()).has_value());
}

TEST(common_JsonHelperTest, ReadsOnlyFiniteJsonNumbers)
{
    EXPECT_EQ(JsonHelper::ToFiniteDouble(12.5), 12.5);
    EXPECT_FALSE(JsonHelper::ToFiniteDouble("12.5").has_value());
    EXPECT_FALSE(JsonHelper::ToFiniteDouble(nullptr).has_value());
    EXPECT_FALSE(JsonHelper::ToFiniteDouble(std::numeric_limits<double>::infinity()).has_value());
}

} // namespace
