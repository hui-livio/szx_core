#include "SzxKeyStore/KeyStore.h"
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "SzxCrypto/Rsa.h"
#if defined(_WIN32)
    #include <windows.h>
#endif
using namespace SZX::KEYSTORE;
TEST(KeyStoreTest, RejectsInvalidReferences)
{
    auto result = openRsaPublicKey({});
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(KeyStoreError::InvalidArgument));
    auto deleted = deleteKey({});
    EXPECT_FALSE(deleted.ok);
    EXPECT_EQ(deleted.error, static_cast<int>(KeyStoreError::InvalidArgument));
    auto badProvider = openRsaPublicKey({"unknown", "key"});
    EXPECT_FALSE(badProvider.ok);
    EXPECT_EQ(badProvider.error, static_cast<int>(KeyStoreError::InvalidArgument));
}
TEST(KeyStoreTest, ReportsPlatformCapability)
{
    auto result = createRsa3072("");
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(KeyStoreError::InvalidArgument));
}

TEST(KeyStoreTest, PersistentKeyLifecycle)
{
#if defined(_WIN32)
    const auto name = std::string("SZX.Test.") + std::to_string(GetCurrentProcessId());
    auto created    = createRsa3072(name, KeyUsage::Sign);
    ASSERT_TRUE(created.ok) << created.message;
    auto opened = openRsaPublicKey({{}, name});
    ASSERT_TRUE(opened.ok) << opened.message;
    EXPECT_EQ(opened.value, created.value);
    auto deleted = deleteKey({{}, name});
    ASSERT_TRUE(deleted.ok) << deleted.message;
    auto missing = openRsaPublicKey({{}, name});
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.error, static_cast<int>(KeyStoreError::NotFound));
#else
    GTEST_SKIP() << "NCrypt lifecycle test requires Windows";
#endif
}

TEST(KeyStoreTest, PersistentKeyDecryptRoundTrip)
{
#if defined(_WIN32)
    const auto name = std::string("SZX.Test.Decrypt.") + std::to_string(GetCurrentProcessId());
    auto created    = createRsa3072(name, KeyUsage::SignAndDecrypt);
    ASSERT_TRUE(created.ok) << created.message;
    auto encrypted = SZX::CRYPTO::rsaOaepSha256Encrypt(created.value, std::vector<std::uint8_t>{'o', 'k'});
    ASSERT_TRUE(encrypted.ok) << encrypted.message;
    auto decrypted = decryptOaepSha256({{}, name}, encrypted.value);
    ASSERT_TRUE(decrypted.ok) << decrypted.message;
    EXPECT_EQ(decrypted.value, (std::vector<std::uint8_t>{'o', 'k'}));
    EXPECT_TRUE(deleteKey({{}, name}).ok);
#else
    GTEST_SKIP() << "NCrypt decrypt test requires Windows";
#endif
}

TEST(KeyStoreTest, EnforcesUsageRestrictions)
{
#if defined(_WIN32)
    const auto signName = std::string("SZX.Test.SignOnly.") + std::to_string(GetCurrentProcessId());
    auto signKey        = createRsa3072(signName, KeyUsage::Sign);
    ASSERT_TRUE(signKey.ok) << signKey.message;
    const std::vector<std::uint8_t> payload{'x'};
    EXPECT_TRUE(signPssSha256({{}, signName}, payload).ok);
    EXPECT_FALSE(decryptOaepSha256({{}, signName}, {}).ok);
    EXPECT_TRUE(deleteKey({{}, signName}).ok);

    const auto decryptName = std::string("SZX.Test.DecryptOnly.") + std::to_string(GetCurrentProcessId());
    auto decryptKey        = createRsa3072(decryptName, KeyUsage::Decrypt);
    ASSERT_TRUE(decryptKey.ok) << decryptKey.message;
    EXPECT_FALSE(signPssSha256({{}, decryptName}, payload).ok);
    EXPECT_TRUE(deleteKey({{}, decryptName}).ok);
#else
    GTEST_SKIP() << "NCrypt usage test requires Windows";
#endif
}
