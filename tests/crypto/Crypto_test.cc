#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <SzxCrypto/Crypto.h>

using namespace SZX::CRYPTO;

TEST(CryptoBase64Test, EncodesAndDecodesStandardBase64)
{
    EXPECT_EQ(encodeBase64(""), "");
    EXPECT_EQ(encodeBase64("f"), "Zg==");
    EXPECT_EQ(encodeBase64("fo"), "Zm8=");
    EXPECT_EQ(encodeBase64("foo"), "Zm9v");
    EXPECT_EQ(encodeBase64("hello"), "aGVsbG8=");

    const auto decoded = decodeBase64("aGVsbG8=");
    ASSERT_TRUE(decoded.ok) << decoded.message;
    EXPECT_EQ(std::string(decoded.value.begin(), decoded.value.end()), "hello");
}

TEST(CryptoBase64Test, SupportsUrlSafeAlphabetAndRejectsInvalidInput)
{
    const std::vector<std::uint8_t> bytes{0xfb, 0xff};
    EXPECT_EQ(encodeBase64(bytes, Base64Alphabet::UrlSafe, false), "-_8");
    const auto decoded = decodeBase64("-_8", Base64Alphabet::UrlSafe);
    ASSERT_TRUE(decoded.ok) << decoded.message;
    EXPECT_EQ(decoded.value, bytes);

    for (const std::string_view input : {"abc$", "A", "====", "AA=A", "abc=def", "-_8"})
    {
        SCOPED_TRACE(input);
        EXPECT_FALSE(decodeBase64(input, Base64Alphabet::Standard).ok);
    }
}

TEST(CryptoHashTest, ComputesDigestAndHmacHexValues)
{
    const auto sha1 = digestHex(HashAlgorithm::Sha1, "abc");
    ASSERT_TRUE(sha1.ok) << sha1.message;
    EXPECT_EQ(sha1.value, "a9993e364706816aba3e25717850c26c9cd0d89d");

    const auto sha256 = digestHex(HashAlgorithm::Sha256, "abc");
    ASSERT_TRUE(sha256.ok) << sha256.message;
    EXPECT_EQ(sha256.value, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    const auto hmac = hmacHex(HashAlgorithm::Sha256, "key", "abc");
    ASSERT_TRUE(hmac.ok) << hmac.message;
    EXPECT_EQ(hmac.value, "9c196e32dc0175f86f4b1cb89289d6619de6bee699e4c378e68309ed97a1a6ab");
}

TEST(CryptoFernetTest, RoundTripsTextAndBinaryPayloads)
{
    constexpr std::string_view key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    const auto encryptedText       = encryptFernet("tensorrt-engine-bytes", key);
    ASSERT_TRUE(encryptedText.ok) << encryptedText.message;
    const auto decryptedText = decryptFernet(encryptedText.value, key);
    ASSERT_TRUE(decryptedText.ok) << decryptedText.message;
    EXPECT_EQ(std::string(decryptedText.value.begin(), decryptedText.value.end()), "tensorrt-engine-bytes");

    const std::string payload{"abc\0def", 7};
    const auto encryptedBinary = encryptFernet(payload, key);
    ASSERT_TRUE(encryptedBinary.ok) << encryptedBinary.message;
    const auto decryptedBinary = decryptFernet(encryptedBinary.value, key);
    ASSERT_TRUE(decryptedBinary.ok) << decryptedBinary.message;
    EXPECT_EQ(decryptedBinary.value, std::vector<std::uint8_t>(payload.begin(), payload.end()));
}
