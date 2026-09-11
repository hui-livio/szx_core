#include "SzxAdmin/Auth/JwtService.h"

#include "SzxCrypto/Crypto.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

using SZX::ADMIN::AUTH::JwtConfig;
using SZX::ADMIN::AUTH::JwtError;
using SZX::ADMIN::AUTH::JwtIssueRequest;
using SZX::ADMIN::AUTH::JwtKeyRecord;
using SZX::ADMIN::AUTH::JwtKeyStore;
using SZX::ADMIN::AUTH::JwtService;
using SZX::ADMIN::AUTH::JwtSigningKey;

namespace
{
class ScopedEnvironment final
{
public:
    ScopedEnvironment(const char *name, const char *value) : name_(name)
    {
#if defined(_WIN32)
        char *current       = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&current, &length, name) == 0 && current != nullptr)
        {
            previous_ = current;
            existed_  = true;
            std::free(current);
        }
#else
        if (const auto *current = std::getenv(name); current != nullptr)
        {
            previous_ = current;
            existed_  = true;
        }
#endif
#if defined(_WIN32)
        EXPECT_EQ(_putenv_s(name, value), 0);
#else
        EXPECT_EQ(setenv(name, value, 1), 0);
#endif
    }

    ~ScopedEnvironment()
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), existed_ ? previous_.c_str() : "");
#else
        if (existed_)
            setenv(name_.c_str(), previous_.c_str(), 1);
        else
            unsetenv(name_.c_str());
#endif
    }

private:
    std::string name_;
    std::string previous_;
    bool existed_{};
};

auto makeKey(SZX::CRYPTO::RsaKeyPair &pair) -> JwtSigningKey
{
    return {.keyId = "test-key-2026", .privateKey = pair.privateKey, .publicKey = pair.publicKey};
}

auto makeBoundKey(SZX::CRYPTO_SERVICE::ICryptoService &crypto, SZX::CRYPTO::RsaKeyPair &pair) -> JwtSigningKey
{
    auto key = makeKey(pair);
    const auto keyId = crypto.publicKeyId(key.publicKey);
    if (keyId.ok)
        key.keyId = keyId.value;
    return key;
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

    auto tampered                                            = issue.value;
    tampered[tampered.find('.', tampered.find('.') + 1) + 1] = tampered.back() == 'A' ? 'B' : 'A';
    const auto verified                                      = service.verify(tampered, makeKey(keys.value));
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

    auto key            = makeKey(keys.value);
    key.keyId           = "rotated-key";
    const auto verified = service.verify(issue.value, key);
    ASSERT_FALSE(verified.ok);
    EXPECT_EQ(verified.error, static_cast<int>(JwtError::KeyRejected));
}

TEST(JwtServiceTest, RotatesSigningKeyAndKeepsPreviousKeyForVerification)
{
    auto firstKeys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(firstKeys.ok) << firstKeys.message;
    auto secondKeys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(secondKeys.ok) << secondKeys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    auto first = makeBoundKey(crypto, firstKeys.value);
    auto second = makeBoundKey(crypto, secondKeys.value);

    JwtKeyStore store(crypto);
    ASSERT_TRUE(store.addKey(JwtKeyRecord{.key = first, .activeForSigning = true}).ok);
    ASSERT_TRUE(store.addKey(JwtKeyRecord{.key = second}).ok);

    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"}, store);
    const auto firstToken = service.issue({.subject = "user-1", .jti = "token-1", .authzVersion = 1});
    ASSERT_TRUE(firstToken.ok) << firstToken.message;

    ASSERT_TRUE(store.activateKey(second.keyId).ok);
    const auto secondToken = service.issue({.subject = "user-1", .jti = "token-2", .authzVersion = 2});
    ASSERT_TRUE(secondToken.ok) << secondToken.message;

    const auto firstClaims = service.verify(firstToken.value);
    ASSERT_TRUE(firstClaims.ok) << firstClaims.message;
    EXPECT_EQ(firstClaims.value.keyId, first.keyId);
    const auto secondClaims = service.verify(secondToken.value);
    ASSERT_TRUE(secondClaims.ok) << secondClaims.message;
    EXPECT_EQ(secondClaims.value.keyId, second.keyId);
}

TEST(JwtKeyStoreTest, RejectsExpiredVerificationKey)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtKeyStore store(crypto);
    const auto result = store.addKey(JwtKeyRecord{.key = makeBoundKey(crypto, keys.value), .notBefore = 100, .expiresAt = 200});
    ASSERT_TRUE(result.ok) << result.message;

    const auto key = crypto.publicKeyId(keys.value.publicKey);
    ASSERT_TRUE(key.ok) << key.message;
    const auto verification = store.verificationKey(key.value, 201);
    ASSERT_FALSE(verification.ok);
    EXPECT_EQ(verification.error, static_cast<int>(JwtError::KeyRejected));
}

TEST(JwtKeyStoreTest, DoesNotExposePrivateKeyWhenReadingVerificationKey)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtKeyStore store(crypto);
    const auto boundKey = makeBoundKey(crypto, keys.value);
    ASSERT_TRUE(store.addKey(JwtKeyRecord{.key = boundKey}).ok);

    const auto verification = store.verificationKey(boundKey.keyId);
    ASSERT_TRUE(verification.ok) << verification.message;
    EXPECT_TRUE(verification.value.privateKey.empty());
    EXPECT_EQ(verification.value.publicKey, keys.value.publicKey);
}

TEST(JwtKeyStoreTest, RejectsFutureActiveSigningKey)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtKeyStore store(crypto);
    const auto result = store.addKey(JwtKeyRecord{.key = makeBoundKey(crypto, keys.value),
                                                  .activeForSigning = true,
                                                  .notBefore = 4'000'000'000});
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(JwtError::KeyRejected));
}

TEST(JwtKeyStoreTest, RejectsKeyIdThatDoesNotMatchPublicKey)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtKeyStore store(crypto);
    auto key = makeKey(keys.value);
    key.keyId = "not-the-public-key-id";

    const auto result = store.addKey(JwtKeyRecord{.key = key});
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(JwtError::KeyRejected));
}

TEST(JwtConfigTest, LoadsEnvironmentConfigurationAndAllowsZeroLeeway)
{
    const ScopedEnvironment issuer("RBAC_JWT_ISSUER", "szx-admin");
    const ScopedEnvironment audience("RBAC_JWT_AUDIENCE", "szx-client");
    const ScopedEnvironment lifetime("RBAC_JWT_LIFETIME_SECONDS", "1200");
    const ScopedEnvironment leeway("RBAC_JWT_CLOCK_LEEWAY_SECONDS", "0");

    const auto config = JwtConfig::loadFromEnvironment();
    ASSERT_TRUE(config.ok) << config.message;
    EXPECT_EQ(config.value.issuer, "szx-admin");
    EXPECT_EQ(config.value.audience, "szx-client");
    EXPECT_EQ(config.value.lifetimeSeconds, 1200);
    EXPECT_EQ(config.value.clockLeewaySeconds, 0);
}

TEST(JwtConfigTest, RejectsInvalidEnvironmentLifetime)
{
    const ScopedEnvironment issuer("RBAC_JWT_ISSUER", "szx-admin");
    const ScopedEnvironment audience("RBAC_JWT_AUDIENCE", "szx-client");
    const ScopedEnvironment lifetime("RBAC_JWT_LIFETIME_SECONDS", "0");

    const auto config = JwtConfig::loadFromEnvironment();
    ASSERT_FALSE(config.ok);
    EXPECT_EQ(config.error, static_cast<int>(JwtError::InvalidArgument));
}

TEST(JwtServiceTest, RejectsAuthzVersionOverflow)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService crypto;
    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = service.issue({.subject = "user-1",
                                      .jti = "token-overflow",
                                      .authzVersion = (std::numeric_limits<std::uint64_t>::max)()},
                                     makeKey(keys.value));
    ASSERT_FALSE(issue.ok);
    EXPECT_EQ(issue.error, static_cast<int>(JwtError::InvalidArgument));

    JwtService overflowingService(crypto,
                                   {.issuer = "szx-admin",
                                    .audience = "szx-client",
                                    .lifetimeSeconds = (std::numeric_limits<std::int64_t>::max)()});
    const auto overflowingIssue = overflowingService.issue({.subject = "user-1", .jti = "token-overflow-2", .authzVersion = 1},
                                                            makeKey(keys.value));
    ASSERT_FALSE(overflowingIssue.ok);
    EXPECT_EQ(overflowingIssue.error, static_cast<int>(JwtError::InvalidArgument));
}

TEST(JwtServiceTest, ReportsCryptoFailureWhenSigningProviderFails)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::FakeCryptoService crypto;
    JwtService service(crypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = service.issue({.subject = "user-1", .jti = "token-crypto", .authzVersion = 1},
                                     makeKey(keys.value));
    ASSERT_FALSE(issue.ok);
    EXPECT_EQ(issue.error, static_cast<int>(JwtError::CryptoFailure));
}

TEST(JwtServiceTest, ReportsCryptoFailureWhenVerificationProviderFails)
{
    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    ASSERT_TRUE(keys.ok) << keys.message;

    SZX::CRYPTO_SERVICE::CryptoService signingCrypto;
    JwtService signingService(signingCrypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto issue = signingService.issue({.subject = "user-1", .jti = "token-crypto-verify", .authzVersion = 1},
                                             makeKey(keys.value));
    ASSERT_TRUE(issue.ok) << issue.message;

    SZX::CRYPTO_SERVICE::FakeCryptoService verificationCrypto;
    JwtService verificationService(verificationCrypto, {.issuer = "szx-admin", .audience = "szx-client"});
    const auto verified = verificationService.verify(issue.value, makeKey(keys.value));
    ASSERT_FALSE(verified.ok);
    EXPECT_EQ(verified.error, static_cast<int>(JwtError::CryptoFailure));
}
