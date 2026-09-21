#include "LoginRepository.h"
#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"

namespace SZX::ADMIN::AUTH
{
inline constexpr auto get_alias_struct_name(LoginUser *)
{
    return std::string_view{"users"};
}
inline constexpr auto get_alias_struct_name(LoginRefresh *)
{
    return std::string_view{"refresh_tokens"};
}
YLT_REFL(LoginUser, id, uid, username, display_name, password_hash, role, status, version, force_password_change,
         temporary_credential_expires_at, failed_login_count, locked_until, authz_version, last_login_at, password_changed_at,
         created_at, updated_at);
REGISTER_CONFLICT_KEY(LoginUser, id)
YLT_REFL(LoginRefresh, id, user_id, token_hash, family_id, created_at, expires_at, ip, replaced_by, last_used_at, revoked_at,
         revocation_reason);
REGISTER_CONFLICT_KEY(LoginRefresh, id)
} // namespace SZX::ADMIN::AUTH

namespace SZX::ADMIN::INFRA::DETAIL
{
struct RefreshRevocation
{
    std::string id;
    std::optional<std::string> revoked_at;
    std::optional<std::string> revocation_reason;
};
inline constexpr auto get_alias_struct_name(RefreshRevocation *)
{
    return std::string_view{"refresh_tokens"};
}
YLT_REFL(RefreshRevocation, id, revoked_at, revocation_reason);
REGISTER_CONFLICT_KEY(RefreshRevocation, id)

using SZX::COMMON::Result;
namespace
{
auto written(bool success, ErrorCode code = ErrorCode::DatabaseQueryFailed) -> Result<void>
{
    return success ? Result<void>::Success() : Result<void>::Failure(static_cast<int>(code), {});
}

} // namespace

auto MySqlLoginRepository::find(const std::string &uid, const std::string &username) -> Result<std::optional<AUTH::LoginUser>>
{
    try
    {
        // Indexed equality narrows candidates; binary equality prevents the schema's
        // accent/case-insensitive collation from authenticating a different UID.
        auto rows = connection_.query_s<AUTH::LoginUser>(
            "((uid_canonical = ? AND BINARY uid_canonical = ?) OR "
            "(username_canonical = ? AND BINARY username_canonical = ?)) ORDER BY id LIMIT 2 FOR UPDATE",
            uid, uid, username, username);
        if (connection_.has_error())
            return Result<std::optional<AUTH::LoginUser>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        if (rows.size() != 1)
            return Result<std::optional<AUTH::LoginUser>>::Success(std::nullopt);
        return Result<std::optional<AUTH::LoginUser>>::Success(std::move(rows.front()));
    } catch (...)
    {
        return Result<std::optional<AUTH::LoginUser>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlLoginRepository::findById(const std::string &id) -> Result<std::optional<AUTH::LoginUser>>
{
    try
    {
        auto rows = connection_.query_s<AUTH::LoginUser>("id = ? FOR UPDATE", id);
        if (connection_.has_error())
            return Result<std::optional<AUTH::LoginUser>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        if (rows.size() != 1)
            return Result<std::optional<AUTH::LoginUser>>::Success(std::nullopt);
        return Result<std::optional<AUTH::LoginUser>>::Success(std::move(rows.front()));
    } catch (...)
    {
        return Result<std::optional<AUTH::LoginUser>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlLoginRepository::revokeRefresh(const std::string &userId, const std::string &now, const std::string &reason)
    -> Result<void>
{
    try
    {
        auto rows = connection_.query_s<RefreshRevocation>("user_id = ? AND revoked_at IS NULL FOR UPDATE", userId);
        if (connection_.has_error())
            return written(false);
        for (auto &row : rows)
        {
            row.revoked_at        = now;
            row.revocation_reason = reason;
            const auto count = connection_.update<&RefreshRevocation::revoked_at, &RefreshRevocation::revocation_reason>(row);
            if (count != 1 || connection_.has_error())
                return written(false);
        }
        return written(true);
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlLoginRepository::saveState(const AUTH::LoginUser &user) -> Result<void>
{
    try
    {
        using AUTH::LoginUser;
        const auto count =
            connection_.update<&LoginUser::status, &LoginUser::version, &LoginUser::authz_version, &LoginUser::failed_login_count,
                               &LoginUser::locked_until, &LoginUser::last_login_at, &LoginUser::temporary_credential_expires_at,
                               &LoginUser::password_hash, &LoginUser::force_password_change, &LoginUser::password_changed_at>(
                user);
        return written(count >= 0 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlLoginRepository::insertRefresh(const AUTH::LoginRefresh &token) -> Result<void>
{
    try
    {
        return written(connection_.insert(token) == 1 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlLoginRepository::findRefresh(const std::string &hash, bool lock) -> Result<std::optional<AUTH::LoginRefresh>>
{
    try
    {
        auto rows = connection_.query_s<AUTH::LoginRefresh>(lock ? "token_hash = ? FOR UPDATE" : "token_hash = ?", hash);
        if (connection_.has_error())
            return Result<std::optional<AUTH::LoginRefresh>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        if (rows.size() != 1)
            return Result<std::optional<AUTH::LoginRefresh>>::Success(std::nullopt);
        return Result<std::optional<AUTH::LoginRefresh>>::Success(std::move(rows.front()));
    } catch (...)
    {
        return Result<std::optional<AUTH::LoginRefresh>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlLoginRepository::saveRefresh(const AUTH::LoginRefresh &token) -> Result<void>
{
    try
    {
        using AUTH::LoginRefresh;
        const auto count = connection_.update<&LoginRefresh::replaced_by, &LoginRefresh::last_used_at, &LoginRefresh::revoked_at,
                                              &LoginRefresh::revocation_reason>(token);
        return written(count == 1 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlLoginRepository::revokeFamily(const std::string &userId, const std::string &familyId, const std::string &now,
                                        const std::string &reason) -> Result<void>
{
    try
    {
        auto rows = connection_.query_s<RefreshRevocation>("user_id = ? AND family_id = ? AND revoked_at IS NULL FOR UPDATE",
                                                           userId, familyId);
        if (connection_.has_error())
            return written(false);
        for (auto &row : rows)
        {
            row.revoked_at        = now;
            row.revocation_reason = reason;
            const auto count = connection_.update<&RefreshRevocation::revoked_at, &RefreshRevocation::revocation_reason>(row);
            if (count != 1 || connection_.has_error())
                return written(false);
        }
        return written(true);
    } catch (...)
    {
        return written(false);
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
