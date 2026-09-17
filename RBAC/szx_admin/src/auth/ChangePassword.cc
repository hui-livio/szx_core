#include "LoginService.h"
#include "PasswordHash.h"

#include "Defer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <limits>

namespace SZX::ADMIN::AUTH
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;
template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}
} // namespace

auto LoginService::changePassword(const ChangePasswordInput &input) -> Result<ChangePasswordResult>
{
    try
    {
        if (!initialized_)
            return failed<ChangePasswordResult>(ErrorCode::ServiceUnavailable);
        if (!validClientPasswordHash(input.oldPasswordHash) || !validClientPasswordHash(input.newPasswordHash) ||
            input.oldPasswordHash == input.newPasswordHash || input.version == 0 ||
            input.version > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) || input.peerAddress.empty() ||
            input.peerAddress.size() > 45)
            return failed<ChangePasswordResult>(ErrorCode::ValidationError);
        const auto claims = jwt_.verify(input.accessToken);
        if (!claims.ok)
            return failed<ChangePasswordResult>(ErrorCode::AuthRequired);
        if (!admit(input.peerAddress))
            return failed<ChangePasswordResult>(ErrorCode::RateLimited);
        ChangePasswordResult output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.loginRepository();
                if (!repository)
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
                const auto found = repository->findById(claims.value.subject);
                if (!found.ok)
                    return Result<void>::Failure(found.error, {});
                if (!found.value || found.value->status != "active" || found.value->authz_version <= 0 ||
                    static_cast<std::uint64_t>(found.value->authz_version) != claims.value.authzVersion)
                    return failed<void>(ErrorCode::AuthRequired);
                auto user = *found.value;
                if (user.version != static_cast<int>(input.version))
                    return failed<void>(ErrorCode::ConflictVersion);
                // The login session authorizes changing a consumed temporary credential.
                // Its expiry is deliberately not rechecked here.
                const auto checked = verifyClientPasswordHash(crypto_, input.oldPasswordHash, user.password_hash);
                if (!checked.ok)
                    return failed<void>(ErrorCode::InternalFailure);
                LoginAudit event{.id             = SZX::COMMON::Uuid::v4().toString(),
                                 .actor_user_id  = user.id,
                                 .target_user_id = user.id,
                                 .event_name     = checked.value ? "auth.password.changed" : "auth.password.change_failed",
                                 .result         = checked.value ? "success" : "failure",
                                 .request_id     = input.requestId,
                                 .ip             = input.peerAddress};
                if (!checked.value)
                {
                    const auto audited = repository->insertAudit(event);
                    if (!audited.ok)
                        return audited;
                    const auto result = connection.commit();
                    committed         = result.ok;
                    return result.ok ? failed<void>(ErrorCode::AuthInvalidCredentials) : result;
                }
                if (user.version == (std::numeric_limits<int>::max)() || user.authz_version == (std::numeric_limits<int>::max)())
                    return failed<void>(ErrorCode::InternalFailure);
                const auto stored = storeClientPasswordHash(crypto_, input.newPasswordHash);
                if (!stored.ok)
                    return Result<void>::Failure(stored.error, {});
                const auto now             = SZX::COMMON::TimeHelper::TimestampToString(SZX::COMMON::TimeHelper::NowTimestamp());
                user.password_hash         = stored.value;
                user.force_password_change = false;
                user.temporary_credential_expires_at.reset();
                user.password_changed_at = now;
                user.failed_login_count  = 0;
                user.locked_until.reset();
                ++user.version;
                ++user.authz_version;
                const auto saved = repository->saveState(user);
                if (!saved.ok)
                    return saved;
                const auto revoked = repository->revokeRefresh(user.id, now);
                if (!revoked.ok)
                    return revoked;
                const auto audited = repository->insertAudit(event);
                if (!audited.ok)
                    return audited;
                const auto result = connection.commit();
                committed         = result.ok;
                if (committed)
                    output = {.version = user.version, .authzVersion = user.authz_version};
                return result;
            });
        return operation.ok ? Result<ChangePasswordResult>::Success(output) :
                              Result<ChangePasswordResult>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<ChangePasswordResult>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::AUTH
