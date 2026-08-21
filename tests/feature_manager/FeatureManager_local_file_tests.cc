/**
 * @file FeatureManager_local_file_tests.cc
 * @brief FeatureManager_local_file_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "FeatureManager_test_support.h"

namespace
{
TEST_P(LocalFileFeatureProviderTest, RoundTripsEveryValueWithoutPrecisionLoss)
{
    FeatureMap expected = {
        {"null", std::monostate{}},
        {"bool", true},
        {"integer", std::int64_t{-9223372036854775807LL}},
        {"number", 1.2345678901234567},
        {"string", std::string{"value: with punctuation"}},
    };

    LocalFileFeatureProvider provider(filePath(), GetParam());
    ASSERT_TRUE(provider.save(expected).ok);
    const auto loaded = provider.load();

    ASSERT_TRUE(loaded.ok) << loaded.message << "\n" << readText(filePath());
    EXPECT_EQ(loaded.value, expected);
}

TEST_P(LocalFileFeatureProviderTest, RejectsMalformedTypedValues)
{
    const bool json = GetParam() == LocalFileFeatureProvider::Format::Json;
    writeText(filePath(),
              json ? R"({"version":1,"encoding":"plain","features":[{"name":"bad","type":"integer","value":"12x"}]})" :
                     "version: 1\nencoding: plain\nfeatures:\n  - name: bad\n    type: integer\n    value: 12x\n");

    const auto loaded = LocalFileFeatureProvider(filePath(), GetParam()).load();
    EXPECT_FALSE(loaded.ok);
}

TEST_P(LocalFileFeatureProviderTest, RejectsDuplicateNames)
{
    const bool json = GetParam() == LocalFileFeatureProvider::Format::Json;
    writeText(
        filePath(),
        json ?
            R"({"version":1,"encoding":"plain","features":[{"name":"same","type":"bool","value":"true"},{"name":"same","type":"bool","value":"false"}]})" :
            "version: 1\nencoding: plain\nfeatures:\n  - name: same\n    type: bool\n    value: true\n  - name: same\n    type: bool\n    value: false\n");

    const auto loaded = LocalFileFeatureProvider(filePath(), GetParam()).load();
    EXPECT_FALSE(loaded.ok);
}

TEST(LocalFileFeatureProviderTest, PlainYamlHexPrefixIsNotDecoded)
{
    const auto directory = tempDirectory();
    const ScopedPathCleanup cleanup(directory);
    const auto path = directory / "features.yaml";
    writeText(path,
              "version: 1\nencoding: plain\nfeatures:\n"
              "  - name: hex_6162\n    type: string\n    value: hex_6364\n");

    const auto loaded = LocalFileFeatureProvider(path, LocalFileFeatureProvider::Format::Yaml).load();
    ASSERT_TRUE(loaded.ok) << loaded.message;
    EXPECT_EQ(std::get<std::string>(loaded.value.at("hex_6162")), "hex_6364");
}

TEST(LocalFileFeatureProviderTest, RejectsUnknownSchemaAndEncoding)
{
    const auto directory = tempDirectory();
    const ScopedPathCleanup cleanup(directory);
    const auto versionPath   = directory / "version.yaml";
    const auto encodingPath  = directory / "encoding.yaml";
    const std::string record = "features:\n  - name: enabled\n    type: bool\n    value: true\n";
    writeText(versionPath, "version: 2\nencoding: plain\n" + record);
    writeText(encodingPath, "version: 1\nencoding: future-v9\n" + record);

    EXPECT_FALSE(LocalFileFeatureProvider(versionPath, LocalFileFeatureProvider::Format::Yaml).load().ok);
    EXPECT_FALSE(LocalFileFeatureProvider(encodingPath, LocalFileFeatureProvider::Format::Yaml).load().ok);
}

TEST(LocalFileFeatureProviderTest, RejectsMissingSchemaVersion)
{
    const auto directory = tempDirectory();
    const ScopedPathCleanup cleanup(directory);
    const auto path = directory / "features.yaml";
    writeText(path,
              "encoding: plain\nfeatures:\n"
              "  - name: enabled\n    type: bool\n    value: true\n");

    const auto loaded = LocalFileFeatureProvider(path, LocalFileFeatureProvider::Format::Yaml).load();

    EXPECT_FALSE(loaded.ok);
}

TEST(LocalFileFeatureProviderTest, RejectsValuesBeyondRoundTripLimitsBeforeWriting)
{
    const auto directory = tempDirectory();
    const ScopedPathCleanup cleanup(directory);
    const auto path = directory / "features.yaml";
    LocalFileFeatureProvider provider(path, LocalFileFeatureProvider::Format::Yaml);
    FeatureMap oversized = {
        {"too-large", std::string(1024U * 1024U + 1U, 'x')},
    };

    const auto saved = provider.save(oversized);

    EXPECT_FALSE(saved.ok);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(LocalFileFeatureProviderTest, WrittenYamlKeepsOrdinaryFeatureReadable)
{
    const auto directory = tempDirectory();
    const ScopedPathCleanup cleanup(directory);
    const auto path = directory / "features.yaml";
    LocalFileFeatureProvider provider(path, LocalFileFeatureProvider::Format::Yaml);

    EXPECT_TRUE(provider.save({{"ui.navigation.enabled", true}}).ok);
    const auto text = readText(path);
    EXPECT_NE(text.find("ui.navigation.enabled"), std::string::npos);
    EXPECT_NE(text.find("bool"), std::string::npos);
    EXPECT_NE(text.find("true"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(JsonAndYaml, LocalFileFeatureProviderTest,
                         testing::Values(LocalFileFeatureProvider::Format::Json, LocalFileFeatureProvider::Format::Yaml));

} // namespace
