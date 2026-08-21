/**
 * @file Crypto_test.cc
 * @brief Crypto_test.cc 覆盖的单元测试与测试辅助。
 */

#include "SzxCrypto/Crypto.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace SZX::CRYPTO;

namespace
{

TEST(CryptoBase64Test, EncodesAndDecodesStandardBase64)
{
    EXPECT_EQ(encodeBase64(""), "");
    EXPECT_EQ(encodeBase64("f"), "Zg==");
    EXPECT_EQ(encodeBase64("fo"), "Zm8=");
    EXPECT_EQ(encodeBase64("foo"), "Zm9v");
    EXPECT_EQ(encodeBase64("hello"), "aGVsbG8=");

    auto decoded = decodeBase64("aGVsbG8=");
    ASSERT_TRUE(decoded.ok) << decoded.message;
    EXPECT_EQ(std::string(decoded.value.begin(), decoded.value.end()), "hello");
}

TEST(CryptoBase64Test, SupportsUrlSafeAlphabetAndOptionalPadding)
{
    const std::vector<std::uint8_t> bytes{0xfb, 0xff};
    EXPECT_EQ(encodeBase64(bytes, Base64Alphabet::UrlSafe), "-_8=");
    EXPECT_EQ(encodeBase64(bytes, Base64Alphabet::UrlSafe, false), "-_8");

    auto decoded = decodeBase64("-_8", Base64Alphabet::UrlSafe);
    ASSERT_TRUE(decoded.ok) << decoded.message;
    EXPECT_EQ(decoded.value, bytes);
}

TEST(CryptoBase64Test, RejectsInvalidInput)
{
    const std::vector<std::string_view> invalidInputs{
        "abc$", "A", "====", "AA=A", "abc=def", "-_8",
    };

    for (const auto input : invalidInputs)
    {
        SCOPED_TRACE(input);

        auto decoded = decodeBase64(input, Base64Alphabet::Standard);

        EXPECT_FALSE(decoded.ok);
    }
}

TEST(CryptoHashTest, ComputesCommonDigestHexValues)
{
    auto sha1 = digestHex(HashAlgorithm::Sha1, "abc");
    ASSERT_TRUE(sha1.ok) << sha1.message;
    EXPECT_EQ(sha1.value, "a9993e364706816aba3e25717850c26c9cd0d89d");

    auto sha256 = digestHex(HashAlgorithm::Sha256, "abc");
    ASSERT_TRUE(sha256.ok) << sha256.message;
    EXPECT_EQ(sha256.value, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    auto sha512 = digestHex(HashAlgorithm::Sha512, "abc");
    ASSERT_TRUE(sha512.ok) << sha512.message;
    EXPECT_EQ(sha512.value,
              "ddaf35a193617abacc417349ae204131"
              "12e6fa4e89a97ea20a9eeee64b55d39a"
              "2192992a274fc1a836ba3c23a3feebbd"
              "454d4423643ce80e2a9ac94fa54ca49f");
}

TEST(CryptoHashTest, ComputesHmacHexValues)
{
    auto mac = hmacHex(HashAlgorithm::Sha256, "key", "abc");
    ASSERT_TRUE(mac.ok) << mac.message;
    EXPECT_EQ(mac.value, "9c196e32dc0175f86f4b1cb89289d6619de6bee699e4c378e68309ed97a1a6ab");
}

TEST(CryptoFernetTest, RoundTripsTextPayload)
{
    constexpr std::string_view key     = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    constexpr std::string_view payload = "tensorrt-engine-bytes";

    auto encrypted = encryptFernet(payload, key);
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = decryptFernet(encrypted.value, key);

    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(std::string(decrypted.value.begin(), decrypted.value.end()), payload);
}

TEST(CryptoFernetTest, RoundTripsBinaryPayload)
{
    constexpr std::string_view key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    const std::string payload{"abc\0def", 7};

    auto encrypted = encryptFernet(payload, key);
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = decryptFernet(encrypted.value, key);

    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(std::vector<std::uint8_t>(payload.begin(), payload.end()), decrypted.value);
}

TEST(CryptoFernetTest, DecryptsSpecCompatibleGoldenToken)
{
    constexpr std::string_view key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    constexpr std::string_view token =
        "gAAAAAAAAAAAAAECAwQFBgcICQoLDA0OD45HabOKVP8npjrd3j2cLZTuCFqEdnuhI13Totj1qsGTM_Uk55goFrk0NV9XahZQBJ11EHV9vrTVR94zQSsnuKI=";
    const std::vector<std::uint8_t> expected{
        'p', 'y', 't', 'h', 'o', 'n', '-', 'f', 'e', 'r', 'n', 'e', 't', '-', 'c', 'o',
        'm', 'p', 'a', 't', 'i', 'b', 'l', 'e', 0,   'e', 'n', 'g', 'i', 'n', 'e',
    };

    auto decrypted = decryptFernet(token, key);

    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(decrypted.value, expected);
}

TEST(CryptoFernetTest, RejectsTamperedTokenSignature)
{
    constexpr std::string_view key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    std::string token =
        "gAAAAAAAAAAAAAECAwQFBgcICQoLDA0OD45HabOKVP8npjrd3j2cLZTuCFqEdnuhI13Totj1qsGTM_Uk55goFrk0NV9XahZQBJ11EHV9vrTVR94zQSsnuKI=";
    token[token.size() - 2] = token[token.size() - 2] == 'A' ? 'B' : 'A';

    auto decrypted = decryptFernet(token, key);

    EXPECT_FALSE(decrypted.ok);
}

TEST(CryptoFernetTest, RejectsInvalidTokenAndKey)
{
    auto invalidToken = decryptFernet("not-a-valid-token", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
    EXPECT_FALSE(invalidToken.ok);

    auto invalidKey = encryptFernet("payload", "bad-key");
    EXPECT_FALSE(invalidKey.ok);
}

} // namespace
