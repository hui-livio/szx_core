#include "SzxCryptoFile/BufferFile.h"
#include <gtest/gtest.h>
#include <vector>
using namespace SZX::CRYPTO_FILE;
TEST(CryptoFileTest, RoundTripsSingleChunk)
{
    const std::vector<std::uint8_t> key(32, 7), data{'s','z','x'};
    auto encrypted = encryptBuffer(key, data);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = decryptBuffer(key, encrypted.value);
    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(decrypted.value, data);
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}
TEST(CryptoFileTest, RejectsTamperedTruncatedAndAppendedContainers)
{
    const std::vector<std::uint8_t> key(32, 9), data{'d','a','t','a'};
    auto encrypted = encryptBuffer(key, data);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto tampered = encrypted.value; tampered.back() ^= 1U;
    EXPECT_FALSE(decryptBuffer(key, tampered).ok);
    auto truncated = encrypted.value; truncated.pop_back();
    EXPECT_FALSE(decryptBuffer(key, truncated).ok);
    auto appended = encrypted.value; appended.push_back(0);
    EXPECT_FALSE(decryptBuffer(key, appended).ok);
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}
TEST(CryptoFileTest, VerifiesWithoutReturningPlaintextAndInspectsHeader)
{
    const std::vector<std::uint8_t> key(32, 3), data{'x','y'};
    auto encrypted = encryptBuffer(key, data);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    EXPECT_TRUE(verifyBuffer(key, encrypted.value).ok);
    auto info = inspectBuffer(encrypted.value);
    ASSERT_TRUE(info.ok) << info.message;
    EXPECT_EQ(info.value.version, 1U);
    EXPECT_EQ(info.value.ciphertextSize, 2U);
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}
TEST(CryptoFileTest, RejectsWrongKeyAndSupportsEmptyPlaintext)
{
    const std::vector<std::uint8_t> key(32, 4), wrongKey(32, 5);
    const std::vector<std::uint8_t> empty;
    auto encrypted = encryptBuffer(key, empty);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    EXPECT_FALSE(verifyBuffer(wrongKey, encrypted.value).ok);
    auto plain = decryptBuffer(key, encrypted.value);
    ASSERT_TRUE(plain.ok) << plain.message;
    EXPECT_TRUE(plain.value.empty());
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}
TEST(CryptoFileTest, RejectsUnknownHeaderFlags)
{
    const std::vector<std::uint8_t> key(32, 6), data{'o','k'};
    auto encrypted = encryptBuffer(key, data);
#if defined(_WIN32)
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    encrypted.value[5] = 1;
    EXPECT_FALSE(inspectBuffer(encrypted.value).ok);
    EXPECT_FALSE(decryptBuffer(key, encrypted.value).ok);
#else
    EXPECT_FALSE(encrypted.ok);
#endif
}
