#include "SzxCryptoService/CryptoService.h"
#include <gtest/gtest.h>
#include <vector>
#include "SzxCrypto/Crypto.h"
using namespace SZX::CRYPTO_SERVICE;
TEST(CryptoServiceTest, ReportsCapabilities)
{
    CryptoService service;
    auto c = service.capabilities();
    EXPECT_TRUE(c.aes256Gcm);
    EXPECT_TRUE(c.rsaOaepSha256);
    EXPECT_TRUE(c.rsaPssSha256);
    EXPECT_TRUE(c.pbkdf2Sha256);
    EXPECT_TRUE(c.argon2id);
    EXPECT_TRUE(c.ps256Jwt);
    EXPECT_TRUE(c.publicKeyId);
}
TEST(CryptoServiceTest, KeepsStableInternalFailureCode)
{
    EXPECT_EQ(static_cast<int>(CryptoServiceError::InternalFailure), 1600);
}
TEST(CryptoServiceTest, FakeTracksCallsAndPropagatesErrors)
{
    FakeCryptoService fake;
    fake.supported.aes256Gcm = true;
    auto r                   = fake.encrypt(std::vector<std::uint8_t>(32), std::vector<std::uint8_t>{1});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(fake.encryptCalls, 1U);
    auto d = fake.decrypt({}, {}, {});
    EXPECT_FALSE(d.ok);
    EXPECT_EQ(fake.decryptCalls, 1U);
    EXPECT_TRUE(fake.capabilities().aes256Gcm);
}
TEST(CryptoServiceTest, EncryptsAndDecryptsThroughFacade)
{
    CryptoService service;
    std::vector<std::uint8_t> key(32, 7), data{1, 2, 3};
    auto e = service.encrypt(key, data);
    ASSERT_TRUE(e.ok) << e.message;
    auto d = service.decrypt(key, e.value);
    ASSERT_TRUE(d.ok) << d.message;
    EXPECT_EQ(d.value, data);
}

TEST(CryptoServiceTest, HashesPasswordThroughRbacFacade)
{
    CryptoService service;
    const std::vector<std::uint8_t> password{'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
    const Argon2idParameters parameters{.memoryCostKiB = 8192, .iterations = 2, .lanes = 1, .saltLength = 16, .hashLength = 32};
    auto encoded = service.hashPassword(password, parameters);
    ASSERT_TRUE(encoded.ok) << encoded.message;
    auto valid = service.verifyPassword(password, encoded.value);
    ASSERT_TRUE(valid.ok) << valid.message;
    EXPECT_TRUE(valid.value);
    auto invalid = service.verifyPassword(std::vector<std::uint8_t>{'n', 'o'}, encoded.value);
    ASSERT_TRUE(invalid.ok) << invalid.message;
    EXPECT_FALSE(invalid.value);
}

TEST(CryptoServiceTest, ExposesPs256AndStablePublicKeyId)
{
    CryptoService service;
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;
    const std::vector<std::uint8_t> signingInput{'j', 'w', 't', '.', 's', 'i', 'g', 'n', 'i',
                                                 'n', 'g', ' ', 'i', 'n', 'p', 'u', 't'};
    auto signature = service.signPs256(keys.value.privateKey, signingInput);
    ASSERT_TRUE(signature.ok) << signature.message;
    auto verified = service.verifyPs256(keys.value.publicKey, signingInput, signature.value);
    ASSERT_TRUE(verified.ok) << verified.message;
    EXPECT_TRUE(verified.value);
    auto keyId = service.publicKeyId(keys.value.publicKey);
    ASSERT_TRUE(keyId.ok) << keyId.message;
    EXPECT_EQ(keyId.value.size(), 64U);
}
