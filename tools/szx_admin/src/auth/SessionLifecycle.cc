#include "LoginService.h"
#include "../audit/AuditRepository.h"
#include "PasswordHash.h"

#include "Defer.hpp"
#include "Logger.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Crypto.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <algorithm>

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
auto validateCookie(const CookieSessionInput &input) -> Result<void>
{
    if (input.refreshToken.size() != 43)
        return failed<void>(ErrorCode::AuthRequired);
    const auto decoded = SZX::CRYPTO::decodeBase64(input.refreshToken, SZX::CRYPTO::Base64Alphabet::UrlSafe);
    if (!decoded.ok || decoded.value.size() != 32)
        return failed<void>(ErrorCode::AuthRequired);
    const auto canonical = SZX::CRYPTO::encodeBase64(decoded.value, SZX::CRYPTO::Base64Alphabet::UrlSafe, false);
    if (!canonical.ok || canonical.value != input.refreshToken)
        return failed<void>(ErrorCode::AuthRequired);
    if (!validClientPasswordHash(input.csrfToken))
        return failed<void>(ErrorCode::Forbidden);
    const auto expected = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, "csrf:" + input.refreshToken);
    if (!expected.ok)
        return failed<void>(ErrorCode::InternalFailure);
    return SZX::CRYPTO::constantTimeEqual(bytes(expected.value), bytes(input.csrfToken)) ? Result<void>::Success() :
                                                                                           failed<void>(ErrorCode::Forbidden);
}
} // namespace

auto LoginService::issueSession(LoginRepository &repository, LoginUser user, const std::string &familyId,
                                const std::string &refreshExpiry, const std::string &peer) -> Result<LoginSession>
{
    const auto expiry = TimeHelper::StringToTimestamp(refreshExpiry.substr(0, 19));
    if (!expiry || user.authz_version <= 0)
        return failed<LoginSession>(ErrorCode::InternalFailure);
    const auto issued = jwt_.issue({.subject      = user.id,
                                    .jti          = SZX::COMMON::Uuid::v4().toString(),
                                    .authzVersion = static_cast<std::uint64_t>(user.authz_version)});
    if (!issued.ok)
        return failed<LoginSession>(ErrorCode::InternalFailure);
    const auto claims = jwt_.verify(issued.value);
    const auto random = SZX::CRYPTO::randomBytes(32);
    if (!claims.ok || !random.ok)
        return failed<LoginSession>(ErrorCode::InternalFailure);
    const auto refresh = SZX::CRYPTO::encodeBase64(random.value, SZX::CRYPTO::Base64Alphabet::UrlSafe, false);
    if (!refresh.ok)
        return failed<LoginSession>(ErrorCode::InternalFailure);
    const auto hash = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, refresh.value);
    const auto csrf = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, "csrf:" + refresh.value);
    if (!hash.ok || !csrf.ok)
        return failed<LoginSession>(ErrorCode::InternalFailure);
    const auto now    = TimeHelper::NowTimestamp();
    const auto stamp  = TimeHelper::TimestampToString(now);
    const auto id     = SZX::COMMON::Uuid::v4().toString();
    const auto stored = repository.insertRefresh(
        {.id         = id,
         .user_id    = user.id,
         .token_hash = hash.value,
         .family_id  = familyId,
         .created_at = stamp,
         .expires_at = refreshExpiry,
         .ip         = peer});
    if (!stored.ok)
        return Result<LoginSession>::Failure(stored.error, {});
    user.password_hash.clear();
    const auto idleExpiry = now + static_cast<std::int64_t>(config_.refreshIdleTimeoutSeconds) * 1000;
    return Result<LoginSession>::Success({.user             = std::move(user),
                                          .accessToken      = issued.value,
                                          .refreshToken     = refresh.value,
                                          .csrfToken        = csrf.value,
                                          .expiresAt        = claims.value.expiresAt,
                                          .refreshId        = id,
                                          .refreshExpiresAt = (std::min)(*expiry, idleExpiry) / 1000});
}

auto LoginService::refresh(const CookieSessionInput &input) -> Result<LoginSession>
{
    return cookieSession(input, false);
}

auto LoginService::logout(const CookieSessionInput &input) -> Result<void>
{
    const auto result = cookieSession(input, true);
    return result.ok ? Result<void>::Success() : Result<void>::Failure(result.error, {});
}

auto LoginService::cookieSession(const CookieSessionInput &input, bool logout) -> Result<LoginSession>
{
    try
    {
        LOGM_DEBUG("szx_admin.auth", "event=session_attempt action={} request_id={} peer={}", logout ? "logout" : "refresh",
                   input.requestId, input.peerAddress);
        if (!initialized_)
            return failed<LoginSession>(ErrorCode::ServiceUnavailable);
        if (input.peerAddress.empty() || input.peerAddress.size() > 45)
            return failed<LoginSession>(ErrorCode::ValidationError);
        const auto valid = validateCookie(input);
        if (!valid.ok)
            return Result<LoginSession>::Failure(valid.error, {});
        if (!admit(input.peerAddress))
            return failed<LoginSession>(ErrorCode::RateLimited);
        const auto hash = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, input.refreshToken);
        if (!hash.ok)
            return failed<LoginSession>(ErrorCode::InternalFailure);
        LoginSession session;
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
                // Resolve the owner without locking the token, preserving user-before-token lock order.
                const auto candidate = repository->findRefresh(hash.value, false);
                if (!candidate.ok)
                    return Result<void>::Failure(candidate.error, {});
                std::optional<LoginUser> user;
                std::optional<LoginRefresh> token;
                if (candidate.value)
                {
                    const auto owner = repository->findById(candidate.value->user_id);
                    if (!owner.ok)
                        return Result<void>::Failure(owner.error, {});
                    user              = owner.value;
                    const auto locked = repository->findRefresh(hash.value, true);
                    if (!locked.ok)
                        return Result<void>::Failure(locked.error, {});
                    token = locked.value;
                    if (token && (!user || token->user_id != user->id))
                        return failed<void>(ErrorCode::AuthRequired);
                }
                const auto now   = TimeHelper::NowTimestamp();
                const auto stamp = TimeHelper::TimestampToString(now);
                std::optional<ErrorCode> denied;
                std::string eventName = logout ? "auth.logout" : "auth.refresh.success";
                if (logout)
                {
                    if (token)
                    {
                        const auto revoked = repository->revokeFamily(token->user_id, token->family_id, stamp, "logout");
                        if (!revoked.ok)
                            return revoked;
                    }
                }
                else if (!token || !user)
                    denied = ErrorCode::AuthRequired;
                else if (token->replaced_by)
                {
                    const auto revoked = repository->revokeFamily(token->user_id, token->family_id, stamp, "replay");
                    if (!revoked.ok)
                        return revoked;
                    denied    = ErrorCode::TokenReplayed;
                    eventName = "auth.refresh.replayed";
                }
                else if (token->revoked_at || user->status != "active")
                    denied = ErrorCode::AuthRequired;
                else
                {
                    const auto expiry  = TimeHelper::StringToTimestamp(token->expires_at.substr(0, 19));
                    const auto created = TimeHelper::StringToTimestamp(token->created_at.substr(0, 19));
                    const auto idleTimeout = static_cast<std::int64_t>(config_.refreshIdleTimeoutSeconds) * 1000;
                    const auto idleExpired = !created || *created > now || now - *created >= idleTimeout;
                    if (!expiry || *expiry <= now || idleExpired)
                        denied = ErrorCode::TokenExpired;
                    else
                    {
                        const auto issued =
                            issueSession(*repository, *user, token->family_id, token->expires_at, input.peerAddress);
                        if (!issued.ok)
                            return Result<void>::Failure(issued.error, {});
                        session                  = issued.value;
                        token->replaced_by       = session.refreshId;
                        token->last_used_at      = stamp;
                        token->revoked_at        = stamp;
                        token->revocation_reason = "rotated";
                        const auto saved         = repository->saveRefresh(*token);
                        if (!saved.ok)
                            return saved;
                    }
                }
                if (denied && eventName == "auth.refresh.success")
                    eventName = "auth.refresh.failure";
                const auto audited =
                    audit->append({.id             = SZX::COMMON::Uuid::v4().toString(),
                                   .actor_user_id  = user ? std::optional<std::string>{user->id} : std::nullopt,
                                   .target_user_id = user ? std::optional<std::string>{user->id} : std::nullopt,
                                   .event_name     = eventName,
                                   .result         = denied ? "denied" : "success",
                                   .request_id     = input.requestId,
                                   .ip             = input.peerAddress});
                if (!audited.ok)
                    return audited;
                const auto result = connection.commit();
                committed         = result.ok;
                if (!result.ok)
                    return result;
                // A replay is a failed request whose family revocation must still commit.
                return denied ? failed<void>(*denied) : Result<void>::Success();
            });
        if (!operation.ok)
        {
            LOGM_DEBUG("szx_admin.auth", "event=session_failed action={} request_id={} code={}", logout ? "logout" : "refresh",
                       input.requestId, operation.error);
            return Result<LoginSession>::Failure(operation.error, {});
        }
        if (logout)
            LOGM_INFO("szx_admin.auth", "event=session_success action=logout request_id={}", input.requestId);
        else
            LOGM_INFO("szx_admin.auth", "event=session_success action=refresh request_id={} user_id={}", input.requestId,
                      session.user.id);
        return Result<LoginSession>::Success(std::move(session));
    } catch (...)
    {
        return failed<LoginSession>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::AUTH
