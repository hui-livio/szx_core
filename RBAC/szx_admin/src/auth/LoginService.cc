#include "LoginService.h"
#include "../HttpSupport.h"
#include "../audit/AuditRepository.h"
#include "../infra/DatabaseInitialization.h"
#include "PasswordHash.h"

#include "Defer.hpp"
#include "FileHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Crypto.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <algorithm>
#include <limits>

namespace SZX::ADMIN::AUTH
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;
using SZX::COMMON::TimeHelper;
template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}
auto bytes(std::string_view value) -> std::span<const std::uint8_t>
{
    return {reinterpret_cast<const std::uint8_t *>(value.data()), value.size()};
}
} // namespace

auto LoginConfig::validate() const -> Result<void>
{
    if (issuer.empty() || issuer.size() > 256 || audience.empty() || audience.size() > 256 || privateKeyPath.empty() ||
        publicKeyPath.empty() || refreshLifetimeSeconds < 900 || refreshLifetimeSeconds > 2592000 ||
        refreshIdleTimeoutSeconds < 900 || refreshIdleTimeoutSeconds > refreshLifetimeSeconds || failedLoginLimit < 1 ||
        failedLoginLimit > 100 || lockSeconds < 1 || lockSeconds > 86400 || attemptsPerMinute < 1 ||
        attemptsPerMinute > 1000 || (allowedOrigin != "*" && !validHttpOrigin(allowedOrigin)))
        return failed<void>(ErrorCode::InvalidConfiguration);
    return Result<void>::Success();
}

LoginService::LoginService(INFRA::DatabasePool &database, LoginConfig config) :
    database_(database),
    config_(std::move(config)),
    keys_(crypto_),
    jwt_(crypto_, {.issuer = config_.issuer, .audience = config_.audience}, keys_),
    rateLimiter_(config_.attemptsPerMinute)
{
}

auto LoginService::initialize() -> Result<void>
{
    try
    {
        if (initialized_)
            return Result<void>::Success();
        const auto valid = config_.validate();
        if (!valid.ok)
            return valid;
        const auto privateFile = SZX::COMMON::FileHelper::ReadBytes(config_.privateKeyPath, 65536);
        const auto publicFile  = SZX::COMMON::FileHelper::ReadBytes(config_.publicKeyPath, 65536);
        if (!privateFile || !publicFile)
            return failed<void>(ErrorCode::InvalidConfiguration);
        auto privateKey = SZX::CRYPTO::rsaPrivateKeyFromEncoded(*privateFile);
        auto publicKey  = SZX::CRYPTO::rsaPublicKeyFromEncoded(*publicFile);
        if (!privateKey.ok || !publicKey.ok)
            return failed<void>(ErrorCode::InvalidConfiguration);
        const auto id = crypto_.publicKeyId(publicKey.value);
        if (!id.ok)
            return failed<void>(ErrorCode::InvalidConfiguration);
        JwtSigningKey key{id.value, std::move(privateKey.value), std::move(publicKey.value)};
        const auto probe = jwt_.issue({.subject = "startup", .jti = SZX::COMMON::Uuid::v4().toString(), .authzVersion = 1}, key);
        if (!probe.ok || !jwt_.verify(probe.value, key).ok)
            return failed<void>(ErrorCode::InvalidConfiguration);
        const auto random = SZX::CRYPTO::randomBytes(32);
        if (!random.ok)
            return failed<void>(ErrorCode::InternalFailure);
        const auto dummy = crypto_.hashPassword(random.value);
        if (!dummy.ok)
            return failed<void>(ErrorCode::InternalFailure);
        const auto added = keys_.addKey({.key = std::move(key), .activeForSigning = true});
        if (!added.ok)
            return failed<void>(ErrorCode::InvalidConfiguration);
        dummyPasswordHash_ = dummy.value;
        initialized_       = true;
        return Result<void>::Success();
    } catch (...)
    {
        return failed<void>(ErrorCode::InternalFailure);
    }
}

auto LoginService::verifyAccess(std::string_view accessToken) -> Result<AccessClaims>
{
    try
    {
        if (!initialized_)
            return failed<AccessClaims>(ErrorCode::ServiceUnavailable);
        const auto claims = jwt_.verify(accessToken);
        if (!claims.ok || claims.value.subject.empty())
            return failed<AccessClaims>(ErrorCode::AuthRequired);
        return Result<AccessClaims>::Success({.subject = claims.value.subject, .authzVersion = claims.value.authzVersion});
    } catch (...)
    {
        return failed<AccessClaims>(ErrorCode::InternalFailure);
    }
}

auto LoginService::admitCredentialIssue(const std::string &peer) -> Result<void>
{
    try
    {
        if (!initialized_)
            return failed<void>(ErrorCode::ServiceUnavailable);
        if (peer.empty() || peer.size() > 45)
            return failed<void>(ErrorCode::ValidationError);
        return admit(peer) ? Result<void>::Success() : failed<void>(ErrorCode::RateLimited);
    } catch (...)
    {
        return failed<void>(ErrorCode::InternalFailure);
    }
}

auto LoginService::admit(const std::string &peer) -> bool
{
    return rateLimiter_.admit(peer);
}

auto LoginService::login(const LoginInput &input) -> Result<LoginSession>
{
    try
    {
        if (!initialized_)
            return failed<LoginSession>(ErrorCode::ServiceUnavailable);
        if (!validClientPasswordHash(input.passwordHash) || input.peerAddress.empty() || input.peerAddress.size() > 45)
            return failed<LoginSession>(ErrorCode::ValidationError);
        if (!admit(input.peerAddress))
        {
            const auto audited = database_.withConnection(
                [&](INFRA::DatabaseConnection &connection) -> Result<void>
                {
                    auto *audit = connection.auditRepository();
                    if (!audit)
                        return failed<void>(ErrorCode::DatabaseQueryFailed);
                    const auto begun = connection.begin();
                    if (!begun.ok)
                        return failed<void>(ErrorCode::AuditWriteFailed);
                    const auto written = audit->append({.id         = SZX::COMMON::Uuid::v4().toString(),
                                                        .event_name = "auth.login.rate_limited",
                                                        .result     = "denied",
                                                        .request_id = input.requestId,
                                                        .ip         = input.peerAddress});
                    if (!written.ok)
                        return written;
                    const auto committed = connection.commit();
                    return committed.ok ? committed : failed<void>(ErrorCode::AuditWriteFailed);
                });
            return audited.ok ? failed<LoginSession>(ErrorCode::RateLimited) : Result<LoginSession>::Failure(audited.error, {});
        }
        const auto uid      = canonicalIdentifier(input.identifier, false);
        const auto username = canonicalIdentifier(input.identifier, true);
        if (!uid.ok || !username.ok)
            return Result<LoginSession>::Failure(!uid.ok ? uid.error : username.error, {});
        LoginSession session;
        bool authenticated{};
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.loginRepository();
                auto *audit      = connection.auditRepository();
                if (!repository || !audit)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto begun = connection.begin();
                if (!begun.ok)
                    return begun;
                bool committed{};
                defer
                {
                    if (!committed)
                    {
                        try
                        {
                            (void)connection.rollback();
                        } catch (...)
                        {
                        }
                    }
                };
                const auto found = repository->find(uid.value, username.value);
                if (!found.ok)
                    return Result<void>::Failure(found.error, {});
                auto user                = found.value;
                const auto &initial      = INFRA::DETAIL::initialAdministrator();
                const bool legacyInitial = user && user->username == initial.username && user->role == initial.role &&
                    user->force_password_change && user->password_hash.starts_with("$argon2id$");
                auto checked = Result<bool>::Success(false);
                if (user && user->password_hash.starts_with(kPasswordHashPrefix))
                    checked = verifyClientPasswordHash(crypto_, input.passwordHash, user->password_hash);
                else if (legacyInitial)
                {
                    // Only upgrade an unchanged initial credential after verifying its old Argon2id hash.
                    // Never interpret the transmitted digest as a legacy plaintext password.
                    checked             = crypto_.verifyPassword(bytes(initial.password), user->password_hash);
                    const auto expected = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, initial.password);
                    if (!expected.ok)
                        return failed<void>(ErrorCode::InternalFailure);
                    if (checked.ok)
                        checked.value = checked.value && input.passwordHash == expected.value;
                }
                else
                {
                    checked       = crypto_.verifyPassword(bytes(input.passwordHash), dummyPasswordHash_);
                    checked.value = false;
                }
                if (!checked.ok)
                    return failed<void>(ErrorCode::InternalFailure);
                const auto now = TimeHelper::NowTimestamp();
                bool expired{};
                if (user && user->temporary_credential_expires_at)
                {
                    const auto expiry = TimeHelper::StringToTimestamp(user->temporary_credential_expires_at->substr(0, 19));
                    expired           = !expiry || *expiry <= now;
                }
                authenticated = user && checked.value && user->status == "active" && !expired;
                AUDIT::AuditRecord event{.id             = SZX::COMMON::Uuid::v4().toString(),
                                         .actor_user_id  = authenticated ? std::optional<std::string>{user->id} : std::nullopt,
                                         .target_user_id = user ? std::optional<std::string>{user->id} : std::nullopt,
                                         .event_name     = authenticated ? "auth.login.success" : "auth.login.failure",
                                         .result         = authenticated ? "success" : "failure",
                                         .request_id     = input.requestId,
                                         .ip             = input.peerAddress};
                bool locked{};
                if (user && user->status == "active")
                {
                    if (authenticated)
                    {
                        if (legacyInitial)
                        {
                            const auto upgraded = storeClientPasswordHash(crypto_, input.passwordHash);
                            if (!upgraded.ok)
                                return Result<void>::Failure(upgraded.error, {});
                            user->password_hash = upgraded.value;
                            event.details_json  = R"({"credential_upgraded":true})";
                        }
                        user->failed_login_count = 0;
                        user->locked_until.reset();
                        user->last_login_at = TimeHelper::TimestampToString(now);
                        // Optional password change must not consume a non-super-admin's credential on login.
                        if (user->temporary_credential_expires_at && user->role == "super_admin")
                            user->temporary_credential_expires_at = TimeHelper::TimestampToString(now);
                        user->updated_at = TimeHelper::TimestampToString(now);
                    }
                    else if (!checked.value)
                    {
                        if (user->failed_login_count < config_.failedLoginLimit)
                            ++user->failed_login_count;
                        if (user->failed_login_count >= config_.failedLoginLimit)
                        {
                            if (user->version == (std::numeric_limits<int>::max)() ||
                                user->authz_version == (std::numeric_limits<int>::max)())
                                return failed<void>(ErrorCode::InternalFailure);
                            user->status = "locked";
                            user->locked_until =
                                TimeHelper::TimestampToString(now + static_cast<std::int64_t>(config_.lockSeconds) * 1000);
                            ++user->version;
                            ++user->authz_version;
                            locked = true;
                        }
                    }
                    const auto saved = repository->saveState(*user);
                    if (!saved.ok)
                        return saved;
                }
                if (authenticated)
                {
                    const auto issued = issueSession(
                        *repository, std::move(*user), SZX::COMMON::Uuid::v4().toString(),
                        TimeHelper::TimestampToString(now + static_cast<std::int64_t>(config_.refreshLifetimeSeconds) * 1000),
                        input.peerAddress);
                    if (!issued.ok)
                        return Result<void>::Failure(issued.error, {});
                    session = issued.value;
                }
                auto audited = audit->append(event);
                if (!audited.ok)
                    return audited;
                if (locked)
                {
                    event.id         = SZX::COMMON::Uuid::v4().toString();
                    event.event_name = "auth.account.locked";
                    event.result     = "success";
                    audited          = audit->append(event);
                    if (!audited.ok)
                        return audited;
                }
                const auto result = connection.commit();
                committed         = result.ok;
                return result;
            });
        if (!operation.ok)
            return Result<LoginSession>::Failure(operation.error, {});
        return authenticated ? Result<LoginSession>::Success(std::move(session)) :
                               failed<LoginSession>(ErrorCode::AuthInvalidCredentials);
    } catch (...)
    {
        return failed<LoginSession>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::AUTH
