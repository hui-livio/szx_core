#include "../auth/LoginRepository.h"
#include "../auth/LoginService.h"
#include "../auth/PasswordHash.h"
#include "AuditDetails.h"
#include "UserService.h"
#include "UserValidation.h"

#include "Defer.hpp"
#include "JsonHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <limits>
#include <string>

namespace SZX::ADMIN::USER
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Json;
using SZX::COMMON::Result;
using SZX::COMMON::TimeHelper;

/** Temporary credentials are read aloud or copied by hand; drop visually ambiguous glyphs. */
constexpr std::string_view kCredentialAlphabet     = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
constexpr std::size_t kCredentialLength            = 20;
constexpr std::int64_t kTemporaryCredentialSeconds = 24 * 60 * 60;

auto now() -> std::string
{
    return TimeHelper::TimestampToString(TimeHelper::NowTimestamp());
}

auto expiry(std::int64_t seconds) -> std::string
{
    return TimeHelper::TimestampToString(TimeHelper::NowTimestamp() + seconds * 1000);
}

/** Bump the optimistic-lock version, and the authorization version when the change invalidates sessions. */
auto bumpVersion(UserRow &user, bool authorization) noexcept -> bool
{
    if (user.version == (std::numeric_limits<int>::max)() ||
        (authorization && user.authz_version == (std::numeric_limits<int>::max)()))
        return false;
    ++user.version;
    if (authorization)
        ++user.authz_version;
    return true;
}

auto writeAudit(AUTH::LoginRepository &sessions, const ActorContext &context, const std::string &actorId,
                const std::string &targetId, std::string_view event, std::string_view result, const Json &details) -> Result<void>
{
    return sessions.insertAudit({.id             = SZX::COMMON::Uuid::v4().toString(),
                                 .actor_user_id  = actorId,
                                 .target_user_id = targetId,
                                 .event_name     = std::string{event},
                                 .result         = std::string{result},
                                 .request_id     = context.requestId,
                                 .ip             = context.peerAddress,
                                 .details_json   = details.dump()});
}

auto reloadUser(UserRepository &users, UserRow &target) -> Result<void>
{
    const auto stored = users.findById(target.id);
    if (!stored.ok)
        return Result<void>::Failure(stored.error, {});
    if (!stored.value)
        return failed<void>(ErrorCode::InternalFailure);
    target = *stored.value;
    return Result<void>::Success();
}
} // namespace

auto UserService::issueCredential(const std::optional<std::string> &passwordHash) -> Result<std::pair<std::string, std::string>>
{
    using Pair = std::pair<std::string, std::string>;
    if (passwordHash)
    {
        const auto stored = AUTH::storeClientPasswordHash(crypto_, *passwordHash);
        if (!stored.ok)
            return Result<Pair>::Failure(stored.error, {});
        return Result<Pair>::Success({{}, stored.value});
    }
    std::string credential;
    credential.reserve(kCredentialLength);
    // Rejection sampling keeps the alphabet uniform instead of biasing low indices.
    constexpr auto limit = static_cast<std::uint8_t>(256 - (256 % kCredentialAlphabet.size()));
    while (credential.size() < kCredentialLength)
    {
        const auto random = SZX::CRYPTO::randomBytes(kCredentialLength);
        if (!random.ok)
            return failed<Pair>(ErrorCode::InternalFailure);
        for (const auto byte : random.value)
        {
            if (byte >= limit)
                continue;
            credential.push_back(kCredentialAlphabet[byte % kCredentialAlphabet.size()]);
            if (credential.size() == kCredentialLength)
                break;
        }
    }
    // The stored record follows the login protocol: Argon2id over the client's SHA-256 digest.
    const auto digest = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, credential);
    if (!digest.ok)
        return failed<Pair>(ErrorCode::InternalFailure);
    const auto stored = AUTH::storeClientPasswordHash(crypto_, digest.value);
    if (!stored.ok)
        return failed<Pair>(ErrorCode::InternalFailure);
    return Result<Pair>::Success({std::move(credential), stored.value});
}

auto UserService::create(const ActorContext &context, const CreateUserInput &input) -> Result<CredentialIssue>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress))
            return failed<CredentialIssue>(ErrorCode::ValidationError);
        if (input.uid.has_value() == input.username.has_value())
            return failed<CredentialIssue>(ErrorCode::UserIdentityOneOfRequired);
        if (!validDisplayName(input.displayName) || !POLICY::validRole(input.role))
            return failed<CredentialIssue>(ErrorCode::ValidationError);
        if (input.passwordHash && !AUTH::validClientPasswordHash(*input.passwordHash))
            return failed<CredentialIssue>(ErrorCode::ValidationError);
        // uid keeps its case; usernames additionally use invariant lowercase for the canonical column.
        const auto display = AUTH::canonicalIdentifier(input.uid ? *input.uid : *input.username, false);
        if (!display.ok)
            return Result<CredentialIssue>::Failure(display.error, {});
        const auto canonical = input.uid ? display : AUTH::canonicalIdentifier(*input.username, true);
        if (!canonical.ok)
            return Result<CredentialIssue>::Failure(canonical.error, {});
        CredentialIssue output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                // Serialize identity reservations across service instances before
                // acquiring any user row locks. UID and username share login input.
                const auto namespaceLock = connection.bootstrapCompleted();
                if (!namespaceLock.ok)
                    return Result<void>::Failure(namespaceLock.error, {});
                if (!namespaceLock.value)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = resolve(context, *users, true);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!POLICY::allows(actor.value.role, POLICY::Capability::UserCreate))
                    return failed<void>(ErrorCode::Forbidden);
                if (!POLICY::mayTarget(actor.value.role, input.role))
                    return failed<void>(ErrorCode::Forbidden);
                const auto uid      = input.uid ? canonical.value : std::string{};
                const auto username = input.uid ? std::string{} : canonical.value;
                const auto existing = users->findByCanonical(uid, username);
                if (!existing.ok)
                    return Result<void>::Failure(existing.error, {});
                if (existing.value)
                    return failed<void>(ErrorCode::UserIdentityConflict);
                const auto admitted = auth_.admitCredentialIssue(context.peerAddress);
                if (!admitted.ok)
                    return admitted;
                const auto credential = issueCredential(input.passwordHash);
                if (!credential.ok)
                    return Result<void>::Failure(credential.error, {});
                const auto expiresAt =
                    input.passwordHash ? std::optional<std::string>{} : std::optional{expiry(kTemporaryCredentialSeconds)};
                const NewUser row{
                    .id                              = SZX::COMMON::Uuid::v4().toString(),
                    .uid                             = input.uid ? std::optional{display.value} : std::nullopt,
                    .uid_canonical                   = input.uid ? std::optional{canonical.value} : std::nullopt,
                    .username                        = input.uid ? std::nullopt : std::optional{display.value},
                    .username_canonical              = input.uid ? std::nullopt : std::optional{canonical.value},
                    .display_name                    = input.displayName,
                    .password_hash                   = credential.value.second,
                    .role                            = input.role,
                    .status                          = "active",
                    .force_password_change           = true,
                    .temporary_credential_expires_at = expiresAt,
                    .created_by                      = actor.value.id,
                };
                if (const auto inserted = users->insert(row); !inserted.ok)
                {
                    // Also translate uniqueness failures from writers outside this service.
                    const auto raced = users->findByCanonical(uid, username);
                    if (raced.ok && raced.value)
                        return failed<void>(ErrorCode::UserIdentityConflict);
                    return Result<void>::Failure(inserted.error, {});
                }
                const auto audited = writeAudit(*sessions, context, actor.value.id, row.id, "user.created", "success",
                                                Json{{"role", row.role}, {"identity", input.uid ? "uid" : "username"}});
                if (!audited.ok)
                    return audited;
                const auto stored = users->findById(row.id);
                if (!stored.ok)
                    return Result<void>::Failure(stored.error, {});
                if (!stored.value)
                    return failed<void>(ErrorCode::InternalFailure);
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = {.user                         = *stored.value,
                              .temporaryCredential          = credential.value.first,
                              .temporaryCredentialExpiresAt = expiresAt ?
                                  TimeHelper::TimestampToIso8601(TimeHelper::StringToTimestamp(*expiresAt).value_or(0)) :
                                  std::string{}};
                return result;
            });
        return operation.ok ? Result<CredentialIssue>::Success(std::move(output)) :
                              Result<CredentialIssue>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<CredentialIssue>(ErrorCode::InternalFailure);
    }
}

auto UserService::updateProfile(const ActorContext &context, const std::string &id, const std::string &displayName,
                                std::uint32_t version) -> Result<UserRow>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validUuid(id) || !validDisplayName(displayName))
            return failed<UserRow>(ErrorCode::ValidationError);
        UserRow output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto authorized = authorize(context, *users, id, POLICY::Capability::UserUpdate, version);
                if (!authorized.ok)
                    return Result<void>::Failure(authorized.error, {});
                auto target = authorized.value.target;
                if (target.display_name != displayName)
                {
                    target.display_name = displayName;
                    // A profile edit changes no authorization input, so sessions stay valid.
                    if (!bumpVersion(target, false))
                        return failed<void>(ErrorCode::InternalFailure);
                    const auto saved = users->save(target);
                    if (!saved.ok)
                        return saved;
                    const auto audited = writeAudit(*sessions, context, authorized.value.actor.id, target.id, "user.updated",
                                                    "success", Json{{"fields", Json::array({"display_name"})}});
                    if (!audited.ok)
                        return audited;
                }
                const auto refreshed = reloadUser(*users, target);
                if (!refreshed.ok)
                    return refreshed;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = target;
                return result;
            });
        return operation.ok ? Result<UserRow>::Success(std::move(output)) : Result<UserRow>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserRow>(ErrorCode::InternalFailure);
    }
}

auto UserService::changeStatus(const ActorContext &context, const std::string &id, const std::string &status,
                               std::uint32_t version) -> Result<UserRow>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validUuid(id) ||
            (status != "active" && status != "disabled"))
            return failed<UserRow>(ErrorCode::ValidationError);
        UserRow output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto authorized = authorize(context, *users, id, POLICY::Capability::UserStatus, version);
                if (!authorized.ok)
                    return Result<void>::Failure(authorized.error, {});
                if (status == "disabled" && authorized.value.actor.id == authorized.value.target.id)
                    return failed<void>(ErrorCode::Forbidden);
                auto target = authorized.value.target;
                if (target.status != status)
                {
                    // Clearing a lock is the unlock endpoint's job; only disabling may bypass it.
                    if (target.status == "locked" && status == "active")
                        return failed<void>(ErrorCode::ValidationError);
                    if (status == "disabled" && target.role == "super_admin" && target.status == "active" &&
                        authorized.value.activeSuperAdmins <= 1)
                        return failed<void>(ErrorCode::LastSuperAdminProtected);
                    const auto stamp    = now();
                    const auto previous = target.status;
                    target.status       = status;
                    if (status == "disabled")
                        target.disabled_at = stamp;
                    else
                    {
                        target.disabled_at.reset();
                        target.locked_until.reset();
                        target.failed_login_count = 0;
                    }
                    if (!bumpVersion(target, true))
                        return failed<void>(ErrorCode::InternalFailure);
                    const auto saved = users->save(target);
                    if (!saved.ok)
                        return saved;
                    const auto revoked = sessions->revokeRefresh(target.id, stamp, "status_changed");
                    if (!revoked.ok)
                        return revoked;
                    const auto audited = writeAudit(*sessions, context, authorized.value.actor.id, target.id,
                                                    "user.status.changed", "success", Json{{"from", previous}, {"to", status}});
                    if (!audited.ok)
                        return audited;
                }
                const auto refreshed = reloadUser(*users, target);
                if (!refreshed.ok)
                    return refreshed;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = target;
                return result;
            });
        return operation.ok ? Result<UserRow>::Success(std::move(output)) : Result<UserRow>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserRow>(ErrorCode::InternalFailure);
    }
}

auto UserService::unlock(const ActorContext &context, const std::string &id, std::uint32_t version) -> Result<UserRow>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validUuid(id))
            return failed<UserRow>(ErrorCode::ValidationError);
        UserRow output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto authorized = authorize(context, *users, id, POLICY::Capability::UserUnlock, version);
                if (!authorized.ok)
                    return Result<void>::Failure(authorized.error, {});
                auto target = authorized.value.target;
                if (target.status != "locked")
                    return failed<void>(ErrorCode::ValidationError);
                const auto stamp          = now();
                target.status             = "active";
                target.failed_login_count = 0;
                target.locked_until.reset();
                target.disabled_at.reset();
                if (!bumpVersion(target, true))
                    return failed<void>(ErrorCode::InternalFailure);
                const auto saved = users->save(target);
                if (!saved.ok)
                    return saved;
                const auto revoked = sessions->revokeRefresh(target.id, stamp, "unlocked");
                if (!revoked.ok)
                    return revoked;
                const auto audited = writeAudit(*sessions, context, authorized.value.actor.id, target.id, "user.unlocked",
                                                "success", Json::object());
                if (!audited.ok)
                    return audited;
                const auto refreshed = reloadUser(*users, target);
                if (!refreshed.ok)
                    return refreshed;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = target;
                return result;
            });
        return operation.ok ? Result<UserRow>::Success(std::move(output)) : Result<UserRow>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserRow>(ErrorCode::InternalFailure);
    }
}

auto UserService::assignRole(const ActorContext &context, const std::string &id, const std::string &role, std::uint32_t version)
    -> Result<UserRow>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validUuid(id) || !POLICY::validRole(role))
            return failed<UserRow>(ErrorCode::ValidationError);
        UserRow output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto authorized = authorize(context, *users, id, POLICY::Capability::UserRoleAssign, version);
                if (!authorized.ok)
                    return Result<void>::Failure(authorized.error, {});
                if (authorized.value.actor.id == authorized.value.target.id)
                    return failed<void>(ErrorCode::Forbidden);
                auto target = authorized.value.target;
                // The new role must also be inside the actor's manageable set.
                if (!POLICY::mayTarget(authorized.value.actor.role, role))
                    return failed<void>(ErrorCode::Forbidden);
                if (target.role != role)
                {
                    if (target.role == "super_admin" && target.status == "active" && authorized.value.activeSuperAdmins <= 1)
                        return failed<void>(ErrorCode::LastSuperAdminProtected);
                    const auto stamp    = now();
                    const auto previous = target.role;
                    target.role         = role;
                    if (!bumpVersion(target, true))
                        return failed<void>(ErrorCode::InternalFailure);
                    const auto saved = users->save(target);
                    if (!saved.ok)
                        return saved;
                    const auto revoked = sessions->revokeRefresh(target.id, stamp, "role_changed");
                    if (!revoked.ok)
                        return revoked;
                    const auto audited = writeAudit(*sessions, context, authorized.value.actor.id, target.id, "user.role.changed",
                                                    "success", Json{{"from", previous}, {"to", role}});
                    if (!audited.ok)
                        return audited;
                }
                const auto refreshed = reloadUser(*users, target);
                if (!refreshed.ok)
                    return refreshed;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = target;
                return result;
            });
        return operation.ok ? Result<UserRow>::Success(std::move(output)) : Result<UserRow>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserRow>(ErrorCode::InternalFailure);
    }
}

auto UserService::resetPassword(const ActorContext &context, const std::string &id, std::uint32_t version,
                                const std::optional<std::string> &passwordHash) -> Result<CredentialIssue>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validUuid(id))
            return failed<CredentialIssue>(ErrorCode::ValidationError);
        if (passwordHash && !AUTH::validClientPasswordHash(*passwordHash))
            return failed<CredentialIssue>(ErrorCode::ValidationError);
        CredentialIssue output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto authorized = authorize(context, *users, id, POLICY::Capability::UserPasswordReset, version);
                if (!authorized.ok)
                    return Result<void>::Failure(authorized.error, {});
                const auto admitted = auth_.admitCredentialIssue(context.peerAddress);
                if (!admitted.ok)
                    return admitted;
                const auto credential = issueCredential(passwordHash);
                if (!credential.ok)
                    return Result<void>::Failure(credential.error, {});
                auto target      = authorized.value.target;
                const auto stamp = now();
                const auto expiresAt =
                    passwordHash ? std::optional<std::string>{} : std::optional{expiry(kTemporaryCredentialSeconds)};
                // A fresh credential deserves a fresh failure counter; the lock state itself
                // stays untouched so unlocking remains an explicit, audited decision.
                target.force_password_change           = true;
                target.temporary_credential_expires_at = expiresAt;
                target.password_changed_at             = stamp;
                target.failed_login_count              = 0;
                if (!bumpVersion(target, true))
                    return failed<void>(ErrorCode::InternalFailure);
                const auto saved = users->save(target);
                if (!saved.ok)
                    return saved;
                const auto stored = users->saveCredential(target.id, credential.value.second);
                if (!stored.ok)
                    return stored;
                const auto revoked = sessions->revokeRefresh(target.id, stamp, "password_reset");
                if (!revoked.ok)
                    return revoked;
                const auto audited = writeAudit(*sessions, context, authorized.value.actor.id, target.id, "user.password.reset",
                                                "success", Json::object());
                if (!audited.ok)
                    return audited;
                const auto refreshed = reloadUser(*users, target);
                if (!refreshed.ok)
                    return refreshed;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = {.user                         = target,
                              .temporaryCredential          = credential.value.first,
                              .temporaryCredentialExpiresAt = expiresAt ?
                                  TimeHelper::TimestampToIso8601(TimeHelper::StringToTimestamp(*expiresAt).value_or(0)) :
                                  std::string{}};
                return result;
            });
        return operation.ok ? Result<CredentialIssue>::Success(std::move(output)) :
                              Result<CredentialIssue>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<CredentialIssue>(ErrorCode::InternalFailure);
    }
}

auto UserService::recordBehavior(const ActorContext &context, const BehaviorEvent &event) -> Result<std::string>
{
    try
    {
        if (!validContext(context.requestId, context.peerAddress) || !validEventName(event.eventName) ||
            reservedEventName(event.eventName) || (event.result != "success" && event.result != "failure") ||
            event.detailsJson.size() > 4096)
            return failed<std::string>(ErrorCode::ValidationError);
        auto details = SZX::COMMON::JsonHelper::ParseRequiredObject(event.detailsJson);
        if (!details)
            return failed<std::string>(ErrorCode::ValidationError);
        redactAuditDetails(*details);
        const auto sanitizedDetails = details->dump();
        std::string identifier;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users    = connection.userRepository();
                auto *sessions = connection.loginRepository();
                if (!users || !sessions)
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
                const auto actor = resolve(context, *users, true);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!behaviorLimiter_.admit(actor.value.id))
                    return failed<void>(ErrorCode::RateLimited);
                const auto id      = SZX::COMMON::Uuid::v4().toString();
                const auto audited = sessions->insertAudit({.id             = id,
                                                            .actor_user_id  = actor.value.id,
                                                            .target_user_id = actor.value.id,
                                                            .event_name     = event.eventName,
                                                            .result         = event.result,
                                                            .request_id     = context.requestId,
                                                            .ip             = context.peerAddress,
                                                            .details_json   = sanitizedDetails});
                if (!audited.ok)
                    return audited;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    identifier = id;
                return result;
            });
        return operation.ok ? Result<std::string>::Success(std::move(identifier)) :
                              Result<std::string>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<std::string>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::USER
