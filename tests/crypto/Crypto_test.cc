/**
 * @file Crypto_test.cc
 * @brief Crypto_test.cc 覆盖的单元测试与测试辅助。
 */

#include "SzxCrypto/Crypto.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
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

TEST(CryptoHashTest, ComputesEmptySha256AndHmac)
{
    auto digestResult = digestHex(HashAlgorithm::Sha256, std::string_view{});
    ASSERT_TRUE(digestResult.ok) << digestResult.message;
    EXPECT_EQ(digestResult.value, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    auto mac = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>{}, std::span<const std::uint8_t>{});
    ASSERT_TRUE(mac.ok) << mac.message;
    EXPECT_EQ(mac.value.size(), 32U);
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

TEST(CryptoAeadTest, MatchesAes256GcmEmptyPlaintextVector)
{
    const std::vector<std::uint8_t> key(32, 0);
    const std::vector<std::uint8_t> nonce(12, 0);
    auto encrypted = aes256GcmEncryptWithNonce(key, nonce, {}, {});
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    EXPECT_TRUE(encrypted.value.ciphertext.empty());
    const std::vector<std::uint8_t> expectedTag{0x53, 0x0f, 0x8a, 0xfb, 0xc7, 0x45, 0x36, 0xb9,
                                                0xa9, 0x63, 0xb4, 0xf1, 0xc4, 0xcb, 0x73, 0x8b};
    EXPECT_EQ(encrypted.value.tag, expectedTag);
#else
    EXPECT_FALSE(encrypted.ok);
    EXPECT_EQ(encrypted.error, static_cast<int>(AeadError::Unsupported));
#endif
}

TEST(CryptoAeadTest, RoundTripsAndRejectsTampering)
{
    const std::vector<std::uint8_t> key(32, 0x11);
    const std::vector<std::uint8_t> plaintext{'l', 'i', 'c', 'e', 'n', 's', 'e'};
    const std::vector<std::uint8_t> aad{'h', 'd', 'r'};
    auto encrypted = aes256GcmEncrypt(key, plaintext, aad);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = aes256GcmDecrypt(key, encrypted.value.nonce, encrypted.value.ciphertext, encrypted.value.tag, aad);
    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(decrypted.value, plaintext);
    encrypted.value.tag.front() ^= 1U;
    auto tampered = aes256GcmDecrypt(key, encrypted.value.nonce, encrypted.value.ciphertext, encrypted.value.tag, aad);
    EXPECT_FALSE(tampered.ok);
    EXPECT_EQ(tampered.error, static_cast<int>(AeadError::AuthenticationFailed));
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}

TEST(CryptoKdfTest, MatchesPbkdf2Sha256Vector)
{
    const std::vector<std::uint8_t> password{'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
    const std::vector<std::uint8_t> salt{'s', 'a', 'l', 't'};
    auto derived = derivePbkdf2Sha256(password, salt, 1, 32);
#if defined(_WIN32)
    ASSERT_TRUE(derived.ok) << derived.message;
    const std::vector<std::uint8_t> expected{0x12, 0x0f, 0xb6, 0xcf, 0xfc, 0xf8, 0xb3, 0x2c, 0x43, 0xe7, 0x22,
                                             0x52, 0x56, 0xc4, 0xf8, 0x37, 0xa8, 0x65, 0x48, 0xc9, 0x2c, 0xcc,
                                             0x35, 0x48, 0x08, 0x05, 0x98, 0x7c, 0xb7, 0x0b, 0xe1, 0x7b};
    EXPECT_EQ(derived.value, expected);
#else
    EXPECT_FALSE(derived.ok);
    EXPECT_EQ(derived.error, static_cast<int>(KdfError::Unsupported));
#endif
}

TEST(CryptoKdfTest, RejectsUnsafeParameters)
{
    const std::vector<std::uint8_t> password{'p'};
    const std::vector<std::uint8_t> salt{'s'};
    EXPECT_FALSE(derivePbkdf2Sha256({}, {}, 1, 32).ok);
    EXPECT_FALSE(derivePbkdf2Sha256(password, salt, 0, 32).ok);
    EXPECT_FALSE(derivePbkdf2Sha256(password, salt, 1, 0).ok);
}

TEST(CryptoRsaTest, GeneratesKeyPairAndRoundTripsOaep)
{
    auto keys = generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;
    ASSERT_FALSE(keys.value.publicKey.empty());
    ASSERT_FALSE(keys.value.privateKey.empty());

    const std::vector<std::uint8_t> plaintext{'r', 's', 'a', '-', 'o', 'a', 'e', 'p'};
    auto encrypted = rsaOaepSha256Encrypt(keys.value.publicKey, plaintext);
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = rsaOaepSha256Decrypt(keys.value.privateKey, encrypted.value);
    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(decrypted.value, plaintext);
}

TEST(CryptoRsaTest, RejectsMalformedKeysAndCiphertext)
{
    const std::vector<std::uint8_t> malformed{1, 2, 3};
    const std::vector<std::uint8_t> plaintext{'x'};
    auto encrypted = rsaOaepSha256Encrypt(malformed, plaintext);
    EXPECT_FALSE(encrypted.ok);
    EXPECT_EQ(encrypted.error, static_cast<int>(RsaError::InvalidArgument));

    auto decrypted = rsaOaepSha256Decrypt(malformed, malformed);
    EXPECT_FALSE(decrypted.ok);
    EXPECT_EQ(decrypted.error, static_cast<int>(RsaError::InvalidArgument));
}

TEST(CryptoRsaTest, RejectsCiphertextWithWrongPrivateKey)
{
    auto first  = generateRsa3072KeyPair();
    auto second = generateRsa3072KeyPair();
    ASSERT_TRUE(first.ok) << first.message;
    ASSERT_TRUE(second.ok) << second.message;
    auto encrypted = rsaOaepSha256Encrypt(first.value.publicKey, std::vector<std::uint8_t>{'x'});
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = rsaOaepSha256Decrypt(second.value.privateKey, encrypted.value);
    EXPECT_FALSE(decrypted.ok);
    EXPECT_EQ(decrypted.error, static_cast<int>(RsaError::AuthenticationFailed));
}

TEST(CryptoRsaTest, ComputesStablePublicKeyId)
{
    auto keys = generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;
    auto first  = rsaPublicKeyId(keys.value.publicKey);
    auto second = rsaPublicKeyId(keys.value.publicKey);
    ASSERT_TRUE(first.ok) << first.message;
    ASSERT_TRUE(second.ok) << second.message;
    EXPECT_EQ(first.value, second.value);
    EXPECT_EQ(first.value.size(), 64U);
    EXPECT_FALSE(rsaPublicKeyId({}).ok);
}

TEST(CryptoRsaTest, ConvertsPublicKeyToDerAndBack)
{
    auto keys = generateRsa3072KeyPair();
#if defined(_WIN32)
    ASSERT_TRUE(keys.ok) << keys.message;
    auto der = rsaPublicKeyToDer(keys.value.publicKey);
    ASSERT_TRUE(der.ok) << der.message;
    ASSERT_FALSE(der.value.empty());
    auto restored = rsaPublicKeyFromDer(der.value);
    ASSERT_TRUE(restored.ok) << restored.message;
    EXPECT_EQ(restored.value, keys.value.publicKey);
    auto derId = rsaPublicKeyIdFromDer(der.value);
    ASSERT_TRUE(derId.ok) << derId.message;
    EXPECT_EQ(derId.value.size(), 64U);
#else
    EXPECT_FALSE(keys.ok);
#endif
}

TEST(CryptoRsaTest, RejectsMalformedDerPublicKey)
{
    const std::vector<std::uint8_t> malformed{0x30, 0x01, 0x00};
    auto result = rsaPublicKeyFromDer(malformed);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(RsaError::InvalidArgument));
    EXPECT_FALSE(rsaPublicKeyIdFromDer(malformed).ok);
}

TEST(CryptoRsaTest, SignsAndRejectsTamperedPssSignature)
{
    auto keys = generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;
    const std::vector<std::uint8_t> data{'l', 'i', 'c', 'e', 'n', 's', 'e'};
    auto signature = rsaPssSha256Sign(keys.value.privateKey, data);
    ASSERT_TRUE(signature.ok) << signature.message;
    auto verified = rsaPssSha256Verify(keys.value.publicKey, data, signature.value);
    ASSERT_TRUE(verified.ok) << verified.message;
    EXPECT_TRUE(verified.value);
    signature.value.front() ^= 0x01U;
    auto tampered            = rsaPssSha256Verify(keys.value.publicKey, data, signature.value);
    ASSERT_TRUE(tampered.ok) << tampered.message;
    EXPECT_FALSE(tampered.value);
}

TEST(CryptoRandomTest, GeneratesRequestedLengthAndComparesConstantTime)
{
    auto first  = randomBytes(32);
    auto second = randomBytes(32);
    ASSERT_TRUE(first.ok) << first.message;
    ASSERT_TRUE(second.ok) << second.message;
    EXPECT_EQ(first.value.size(), 32U);
    EXPECT_EQ(second.value.size(), 32U);
    EXPECT_TRUE(constantTimeEqual(first.value, first.value));
    EXPECT_FALSE(constantTimeEqual(first.value, std::vector<std::uint8_t>(32, 0)));
    EXPECT_FALSE(constantTimeEqual(first.value, std::vector<std::uint8_t>(31, 0)));
    EXPECT_TRUE(constantTimeEqual({}, {}));
}

TEST(CryptoRandomTest, SupportsZeroLengthRandomRequest)
{
    auto result = randomBytes(0);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_TRUE(result.value.empty());
}

TEST(CryptoSecureBytesTest, ClearsAndMovesSecretData)
{
    SecureBytes secret(std::vector<std::uint8_t>{1, 2, 3});
    EXPECT_EQ(secret.size(), 3U);
    auto moved = std::move(secret).take();
    EXPECT_EQ(moved, (std::vector<std::uint8_t>{1, 2, 3}));
}

TEST(CryptoSecureBytesTest, ClearsAndMoveAssigns)
{
    SecureBytes source(std::vector<std::uint8_t>{9, 8, 7});
    SecureBytes target(std::vector<std::uint8_t>{1});
    target = std::move(source);
    EXPECT_EQ(std::vector<std::uint8_t>(target.span().begin(), target.span().end()), (std::vector<std::uint8_t>{9, 8, 7}));
    EXPECT_TRUE(source.empty());
    target.clear();
    EXPECT_TRUE(target.empty());
    EXPECT_TRUE(target.span().empty());
}

} // namespace
