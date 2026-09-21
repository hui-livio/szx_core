#include "../../RBAC/szx_admin/src/auth/LoginService.h"
#include "../../RBAC/szx_admin/src/auth/PasswordHash.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <gtest/gtest.h>

using namespace SZX::ADMIN;

TEST(AdminLoginTest, StoresSaltedArgon2idOfClientDigestAndRejectsPlaintext)
{
    SZX::CRYPTO_SERVICE::CryptoService crypto;
    const std::string digest(64, 'a');
    const auto stored = AUTH::storeClientPasswordHash(crypto, digest);
    ASSERT_TRUE(stored.ok);
    EXPECT_TRUE(stored.value.starts_with("sha256-v1:$argon2id$"));
    EXPECT_NE(stored.value, digest);
    const auto verified = AUTH::verifyClientPasswordHash(crypto, digest, stored.value);
    ASSERT_TRUE(verified.ok);
    EXPECT_TRUE(verified.value);
    const auto wrong = AUTH::verifyClientPasswordHash(crypto, std::string(64, 'b'), stored.value);
    ASSERT_TRUE(wrong.ok);
    EXPECT_FALSE(wrong.value);
    EXPECT_FALSE(AUTH::verifyClientPasswordHash(crypto, digest, stored.value.substr(AUTH::kPasswordHashPrefix.size())).ok);
    for (const auto &invalid : {std::string{"superadmin"}, std::string(63, 'a'), std::string(65, 'a'), std::string(64, 'A'),
                                std::string(64, 'g'), std::string{}})
    {
        EXPECT_FALSE(AUTH::validClientPasswordHash(invalid));
        EXPECT_EQ(AUTH::storeClientPasswordHash(crypto, invalid).error, static_cast<int>(INFRA::ErrorCode::ValidationError));
        EXPECT_EQ(AUTH::verifyClientPasswordHash(crypto, invalid, stored.value).error,
                  static_cast<int>(INFRA::ErrorCode::ValidationError));
    }
}

TEST(AdminLoginTest, CanonicalizesUnicodeWithoutMakingUidCaseInsensitive)
{
    const auto user = AUTH::canonicalIdentifier(" E\xCC\x81NGINEER ", true);
    ASSERT_TRUE(user.ok);
    EXPECT_EQ(user.value, "\xC3\xA9ngineer");
    const auto uid = AUTH::canonicalIdentifier(" U10086 ", false);
    ASSERT_TRUE(uid.ok);
    EXPECT_EQ(uid.value, "U10086");
    const auto supplementary = AUTH::canonicalIdentifier("\xF0\x9F\x98\x80", false);
    ASSERT_TRUE(supplementary.ok);
    EXPECT_EQ(supplementary.value, "\xF0\x9F\x98\x80");
    EXPECT_TRUE(AUTH::canonicalIdentifier(std::string(64, 'a'), true).ok);
    for (const auto &invalid : {std::string{}, std::string{" \t"}, std::string{"a\nb"}, std::string{"\xff"},
                                std::string{"a\0b", 3}, std::string(65, 'a'), std::string(513, ' ')})
    {
        const auto result = AUTH::canonicalIdentifier(invalid, true);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(INFRA::ErrorCode::ValidationError));
        EXPECT_TRUE(result.message.empty());
    }
}

TEST(AdminLoginTest, RejectsInvalidLimitsAndMissingKeysBeforeServingLogin)
{
    AUTH::LoginConfig config;
    EXPECT_EQ(config.allowedOrigin, "*");
    ASSERT_TRUE(config.validate().ok);
    config.failedLoginLimit = 0;
    EXPECT_FALSE(config.validate().ok);
    config.failedLoginLimit  = 5;
    config.attemptsPerMinute = 0;
    EXPECT_FALSE(config.validate().ok);
    config.attemptsPerMinute      = 30;
    config.refreshLifetimeSeconds = 899;
    EXPECT_FALSE(config.validate().ok);
    config.refreshLifetimeSeconds = 604800;
    config.refreshIdleTimeoutSeconds = 899;
    EXPECT_FALSE(config.validate().ok);
    config.refreshIdleTimeoutSeconds = 604801;
    EXPECT_FALSE(config.validate().ok);
    config.refreshIdleTimeoutSeconds = 1800;
    config.allowedOrigin          = "null";
    EXPECT_FALSE(config.validate().ok);
    for (const auto origin :
         {"", "https://", "https://example.test/", "https://example.test/path", "https://user@example.test",
          "https://*.example.test", "https://example.test\r\nInjected: 1", "https://example.test https://evil.test"})
    {
        config.allowedOrigin = origin;
        EXPECT_FALSE(config.validate().ok) << origin;
    }
    config.allowedOrigin  = "https://admin.example.test";
    config.privateKeyPath = "missing-login-test-private-key.pem";
    config.publicKeyPath  = "missing-login-test-public-key.pem";
    INFRA::DatabasePool database(INFRA::DatabaseConfig{});
    AUTH::LoginService login(database, config);
    const auto initialized = login.initialize();
    EXPECT_FALSE(initialized.ok);
    EXPECT_EQ(initialized.error, static_cast<int>(INFRA::ErrorCode::InvalidConfiguration));
    EXPECT_TRUE(initialized.message.empty());
    const auto result = login.login(
        {.identifier = "admin", .passwordHash = std::string(64, 'a'), .requestId = "test", .peerAddress = "127.0.0.1"});
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(INFRA::ErrorCode::ServiceUnavailable));
    EXPECT_TRUE(result.message.empty());
}
