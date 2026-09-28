#include "UserRepository.h"
#include "../auth/LoginService.h"
#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "JsonHelper.hpp"
#include "UUID.hpp"

#include <algorithm>
#include <climits>
#include <string>
#include <tuple>
#include <utility>

namespace SZX::ADMIN::USER
{
inline constexpr auto get_alias_struct_name(UserRow *)
{
    return std::string_view{"users"};
}
inline constexpr auto get_alias_struct_name(NewUser *)
{
    return std::string_view{"users"};
}
YLT_REFL(UserRow, id, login_identifier, login_identifier_canonical, identifier_type, display_name, role_id, role, status, version, force_password_change, temporary_credential_expires_at,
         failed_login_count, locked_until, password_changed_at, disabled_at, created_by, authz_version, last_login_at, created_at,
         updated_at);
REGISTER_CONFLICT_KEY(UserRow, id)
YLT_REFL(NewUser, id, login_identifier, login_identifier_canonical, identifier_type,
         display_name, password_hash, role_id, role, status,
         force_password_change, temporary_credential_expires_at, created_by);
} // namespace SZX::ADMIN::USER

namespace SZX::ADMIN::INFRA::DETAIL
{
struct UserCredential
{
    std::string id;
    std::string password_hash;
};
inline constexpr auto get_alias_struct_name(UserCredential *)
{
    return std::string_view{"users"};
}
YLT_REFL(UserCredential, id, password_hash);
REGISTER_CONFLICT_KEY(UserCredential, id)

using SZX::COMMON::Result;
namespace
{
constexpr std::string_view kSuperAdmin = "super_admin";
constexpr std::string_view kActive     = "active";

template <typename T>
auto queryFailed() -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
}

auto written(bool success) -> Result<void>
{
    return success ? Result<void>::Success() : Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
}

/** Neutralize LIKE metacharacters so a search term is matched literally. */
auto escapeLike(std::string_view value) -> std::string
{
    std::string result;
    result.reserve(value.size() + 2);
    for (const auto character : value)
    {
        if (character == '\\' || character == '%' || character == '_')
            result.push_back('\\');
        result.push_back(character);
    }
    return result;
}

/** Compiled sort allowlist; a deterministic id tiebreaker keeps pagination stable. */
auto orderBy(USER::UserSort sort) noexcept -> std::string_view
{
    switch (sort)
    {
    case USER::UserSort::CreatedAtAsc:
        return " ORDER BY created_at ASC, id ASC";
    case USER::UserSort::DisplayNameAsc:
        return " ORDER BY display_name ASC, id ASC";
    case USER::UserSort::DisplayNameDesc:
        return " ORDER BY display_name DESC, id ASC";
    case USER::UserSort::LastLoginAtDesc:
        return " ORDER BY last_login_at DESC, id ASC";
    case USER::UserSort::LastLoginAtAsc:
        return " ORDER BY last_login_at ASC, id ASC";
    case USER::UserSort::RoleAsc:
        return " ORDER BY role ASC, id ASC";
    case USER::UserSort::StatusAsc:
        return " ORDER BY status ASC, id ASC";
    case USER::UserSort::CreatedAtDesc:
        break;
    }
    return " ORDER BY created_at DESC, id ASC";
}

} // namespace

auto MySqlUserRepository::lockForMutation(const std::string &actorId, const std::string &targetId) -> Result<USER::MutationLock>
{
    try
    {
        const std::string superAdmin{kSuperAdmin};
        const std::string active{kActive};
        const auto actorUuid  = SZX::COMMON::Uuid::parse(actorId);
        const auto targetUuid = SZX::COMMON::Uuid::parse(targetId);
        if (!actorUuid || !targetUuid)
            return Result<USER::MutationLock>::Failure(static_cast<int>(ErrorCode::ValidationError), {});
        // A single id-ordered locking read takes every row this mutation can need, so
        // requests that target each other cannot deadlock on interleaved lock order.
        auto rows = connection_.query_s<USER::UserRow>("(id = ? OR id = ? OR (role = ? AND status = ?)) ORDER BY id FOR UPDATE",
                                                       actorId, targetId, superAdmin, active);
        if (connection_.has_error())
            return queryFailed<USER::MutationLock>();
        USER::MutationLock locked;
        for (auto &row : rows)
        {
            if (row.role == kSuperAdmin && row.status == kActive)
                ++locked.activeSuperAdmins;
            const auto rowUuid = SZX::COMMON::Uuid::parse(row.id);
            if (rowUuid == actorUuid)
                locked.actor = row;
            if (rowUuid == targetUuid)
                locked.target = std::move(row);
        }
        return Result<USER::MutationLock>::Success(std::move(locked));
    } catch (...)
    {
        return queryFailed<USER::MutationLock>();
    }
}

auto MySqlUserRepository::findById(const std::string &id, bool lock) -> Result<std::optional<USER::UserRow>>
{
    try
    {
        auto rows = connection_.query_s<USER::UserRow>(lock ? "id = ? FOR UPDATE" : "id = ?", id);
        if (connection_.has_error())
            return queryFailed<std::optional<USER::UserRow>>();
        if (rows.size() != 1)
            return Result<std::optional<USER::UserRow>>::Success(std::nullopt);
        return Result<std::optional<USER::UserRow>>::Success(std::move(rows.front()));
    } catch (...)
    {
        return queryFailed<std::optional<USER::UserRow>>();
    }
}

auto MySqlUserRepository::findByCanonical(const std::string &canonicalValue)
    -> Result<std::optional<USER::UserRow>>
{
    try
    {
        auto rows = connection_.query_s<USER::UserRow>("login_identifier_canonical = ? ORDER BY id LIMIT 1", canonicalValue);
        if (connection_.has_error())
            return queryFailed<std::optional<USER::UserRow>>();
        if (!rows.empty())
            return Result<std::optional<USER::UserRow>>::Success(std::move(rows.front()));
        return Result<std::optional<USER::UserRow>>::Success(std::nullopt);
    } catch (...)
    {
        return queryFailed<std::optional<USER::UserRow>>();
    }
}

auto MySqlUserRepository::list(const USER::UserFilter &filter) -> Result<USER::UserPage>
{
    try
    {
        if (filter.roles.empty() || filter.limit == 0)
            return Result<USER::UserPage>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        // A JSON table keeps the role allowlist parameterized without imposing a
        // compile-time limit on the number of roles an administrator can manage.
        const auto roles = SZX::COMMON::Json(filter.roles).dump();
        const auto status = filter.status;
        const auto search = filter.search;
        const auto needle = search.empty() ? std::string{} : "%" + escapeLike(search) + "%";
        const auto limit  = static_cast<std::uint64_t>(filter.limit);
        const auto offset = filter.offset;
        const std::string source{" FROM users u JOIN JSON_TABLE(?, '$[*]' COLUMNS (code VARCHAR(64) PATH '$')) "
                                 "allowed ON allowed.code = u.role WHERE "};
        const std::string predicate{"(? = '' OR u.status = ?) AND "
                                    "(? = '' OR u.display_name LIKE ? OR u.login_identifier LIKE ?)"};

        const auto totals =
            connection_.query_s<std::tuple<std::uint64_t>>("SELECT COUNT(*)" + source + predicate, roles, status, status,
                                                           search, needle, needle);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<USER::UserPage>();

        USER::UserPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total > offset)
        {
            auto rows = connection_.query_s<USER::UserRow>("SELECT u.*" + source + predicate +
                                                           std::string{orderBy(filter.sort)} + " LIMIT ? OFFSET ?",
                                                           roles, status, status, search, needle, needle, limit, offset);
            if (connection_.has_error())
                return queryFailed<USER::UserPage>();
            page.items = std::move(rows);
        }
        return Result<USER::UserPage>::Success(std::move(page));
    } catch (...)
    {
        return queryFailed<USER::UserPage>();
    }
}

auto MySqlUserRepository::insert(const USER::NewUser &user) -> Result<void>
{
    try
    {
        return written(connection_.insert(user) == 1 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlUserRepository::save(const USER::UserRow &user) -> Result<void>
{
    try
    {
        using USER::UserRow;
        const auto count = connection_.update<&UserRow::display_name, &UserRow::role, &UserRow::status, &UserRow::version,
                                              &UserRow::force_password_change, &UserRow::temporary_credential_expires_at,
                                              &UserRow::failed_login_count, &UserRow::locked_until, &UserRow::password_changed_at,
                                              &UserRow::disabled_at, &UserRow::authz_version>(user);
        return written(count == 1 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

auto MySqlUserRepository::saveCredential(const std::string &id, const std::string &passwordHash) -> Result<void>
{
    try
    {
        const auto count = connection_.update<&UserCredential::password_hash>(UserCredential{id, passwordHash});
        return written(count == 1 && !connection_.has_error());
    } catch (...)
    {
        return written(false);
    }
}

} // namespace SZX::ADMIN::INFRA::DETAIL
