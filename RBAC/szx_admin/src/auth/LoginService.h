#pragma once

#include "../RateLimiter.h"
#include "LoginRepository.h"
#include "SzxAdmin/Auth/JwtService.h"
#include "SzxAdmin/Infra/Database.h"

namespace SZX::ADMIN::AUTH
{
struct LoginConfig
{
    std::string issuer{"szx-admin"};
    std::string audience{"szx-client"};
    std::string privateKeyPath{"config/keys/jwt-private.pem"};
    std::string publicKeyPath{"config/keys/jwt-public.pem"};
    std::string allowedOrigin{"*"};
    int refreshLifetimeSeconds{604800};
    int failedLoginLimit{5};
    int lockSeconds{900};
    int attemptsPerMinute{30};
    [[nodiscard]] auto validate() const -> SZX::COMMON::Result<void>;
};
SZX_REFL(LoginConfig, issuer, audience, privateKeyPath, publicKeyPath, allowedOrigin, refreshLifetimeSeconds, failedLoginLimit,
         lockSeconds, attemptsPerMinute);

struct LoginInput
{
    std::string identifier;
    std::string passwordHash;
    std::string requestId;
    std::string peerAddress;
};

struct LoginSession
{
    LoginUser user;
    std::string accessToken;
    std::string refreshToken;
    std::string csrfToken;
    std::int64_t expiresAt{};
    std::string refreshId;
    std::int64_t refreshExpiresAt{};
};

struct CookieSessionInput
{
    std::string refreshToken;
    std::string csrfToken;
    std::string requestId;
    std::string peerAddress;
};

struct ChangePasswordInput
{
    std::string accessToken;
    std::string oldPasswordHash;
    std::string newPasswordHash;
    std::uint32_t version{};
    std::string requestId;
    std::string peerAddress;
};

struct ChangePasswordResult
{
    int version{};
    int authzVersion{};
};

/** Access JWT claims needed by protected endpoints. */
struct AccessClaims
{
    std::string subject;
    std::uint64_t authzVersion{};
};

/** NFC identifier normalization; usernames additionally use invariant lowercase. */
[[nodiscard]] auto canonicalIdentifier(std::string_view input, bool username) -> SZX::COMMON::Result<std::string>;

/** Initialize once before accepting requests. Database must outlive this service. */
class LoginService final
{
public:
    LoginService(INFRA::DatabasePool &database, LoginConfig config);
    [[nodiscard]] auto initialize() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto login(const LoginInput &input) -> SZX::COMMON::Result<LoginSession>;
    /** Authenticated self-service change, also allowed during the initial password-change flow.
     * Success revokes all sessions; callers must log in again. */
    [[nodiscard]] auto changePassword(const ChangePasswordInput &input) -> SZX::COMMON::Result<ChangePasswordResult>;
    /** Rotate a refresh token atomically. A replay revokes its entire refresh family. */
    [[nodiscard]] auto refresh(const CookieSessionInput &input) -> SZX::COMMON::Result<LoginSession>;
    /** Idempotently revoke the cookie's family. Already-issued access JWTs retain their normal lifetime. */
    [[nodiscard]] auto logout(const CookieSessionInput &input) -> SZX::COMMON::Result<void>;
    /** Verify signature, issuer/audience and validity window only. Callers must still check the
     * database user status and authz_version, ideally inside their own transaction. */
    [[nodiscard]] auto verifyAccess(std::string_view accessToken) -> SZX::COMMON::Result<AccessClaims>;
    /** Apply the shared per-peer authentication budget before issuing a credential.
     * Call only after checking the actor's current database permissions. */
    [[nodiscard]] auto admitCredentialIssue(const std::string &peer) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto config() const noexcept -> const LoginConfig &
    {
        return config_;
    }

private:
    auto admit(const std::string &peer) -> bool;
    auto cookieSession(const CookieSessionInput &input, bool logout) -> SZX::COMMON::Result<LoginSession>;
    // Called inside the caller's transaction and exception boundary, with the user row locked.
    auto issueSession(LoginRepository &repository, LoginUser user, const std::string &familyId, const std::string &refreshExpiry,
                      const std::string &peer) -> SZX::COMMON::Result<LoginSession>;
    INFRA::DatabasePool &database_;
    LoginConfig config_;
    SZX::CRYPTO_SERVICE::CryptoService crypto_;
    JwtKeyStore keys_;
    JwtService jwt_;
    std::string dummyPasswordHash_;
    bool initialized_{};
    RateLimiter rateLimiter_;
};
} // namespace SZX::ADMIN::AUTH
