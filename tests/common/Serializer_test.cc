/**
 * @file Serializer_test.cc
 * @brief Serializer_test.cc 覆盖的单元测试与测试辅助。
 */

#include "Serializer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
using namespace SZX::COMMON;

TEST(common_SerializerTest, JsonAliasBuildsAndParsesStructuredData)
{
    Json json;
    json["name"]    = "pcb";
    json["enabled"] = true;
    json["roi"]     = {{"x", 10}, {"y", 20}, {"width", 30}, {"height", 40}};

    const auto text   = json.dump();
    const auto parsed = Json::parse(text);

    EXPECT_EQ(parsed["name"], "pcb");
    EXPECT_TRUE(parsed["enabled"].get<bool>());
    EXPECT_EQ(parsed["roi"]["width"], 30);
}

TEST(common_SerializerTest, IguanaNamespaceAliasSerializesContainers)
{
    const std::vector<int> input{1, 2, 3};
    std::string text;

    Serializer::to_json(input, text);
    EXPECT_EQ(text, "[1,2,3]");

    std::vector<int> output;
    Serializer::from_json(output, text);
    EXPECT_EQ(output, input);
}
} // namespace
