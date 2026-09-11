#include "SzxAdmin/Auth/JwtService.h"

#include "SzxCrypto/Crypto.h"

#include <gtest/gtest.h>

#include <string>

using SZX::ADMIN::AUTH::JwtConfig;
using SZX::ADMIN::AUTH::JwtError;
using SZX::ADMIN::AUTH::JwtIssueRequest;
using SZX::ADMIN::AUTH::JwtService;
using SZX::ADMIN::AUTH::JwtSigningKey;

namespace
{
auto makeKey(SZX::CRYPTO::RsaKeyPair &pair) -> JwtSigningKey
{
    return {.keyId = "test-key-2026", .privateKey = pair.privateKey, .publicKey = pair.publicKey};
}
} // namespace

TEST(JwtServiceTest, IssuesAndVerifiesPs256TokenThroughCryptoService)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = service.issue({.subject = "user-1", .jti = "token-1", .authzVersion = 7}, makeKey(keys.value));
    ASSERT_TRUE(issue.ok) << issue.message;

    const auto verified = service.verify(issue.value, makeKey(keys.value));
    ASSERT_TRUE(verified.ok) << verified.message;
    EXPECT_EQ(verified.value.keyId, "test-key-2026");
    EXPECT_EQ(verified.value.issuer, "szx-admin");
    EXPECT_EQ(verified.value.audience, "szx-client");
    EXPECT_EQ(verified.value.subject, "user-1");
    EXPECT_EQ(verified.value.jti, "token-1");
    EXPECT_EQ(verified.value.authzVersion, 7U);
}

TEST(JwtServiceTest, RejectsTamperedToken)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = service.issue({.subject = "user-1", .jti = "token-1", .authzVersion = 1}, makeKey(keys.value));
    ASSERT_TRUE(issue.ok) << issue.message;

    auto tampered = issue.value;
    tampered[tampered.find('.', tampered.find('.') + 1) + 1] = tampered.back() == 'A' ? 'B' : 'A';
    const auto verified = service.verify(tampered, makeKey(keys.value));
    ASSERT_FALSE(verified.ok);
    EXPECT_EQ(verified.error, static_cast<int>(JwtError::SignatureInvalid));
}

TEST(JwtServiceTest, RejectsWrongKeyId)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = service.issue({.subject = "user-1", .jti = "token-1", .authzVersion = 1}, makeKey(keys.value));
    ASSERT_TRUE(issue.ok) << issue.message;

    auto key = makeKey(keys.value);
    key.keyId = "rotated-key";
    const auto verified = service.verify(issue.value, key);
    ASSERT_FALSE(verified.ok);
    EXPECT_EQ(verified.error, static_cast<int>(JwtError::KeyRejected));
}
