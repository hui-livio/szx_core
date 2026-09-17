#include "UserRepository.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "UUID.hpp"

#include <ormpp/mysql.hpp>

#include <array>
#include <climits>
#include <string>
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
inline constexpr auto get_alias_struct_name(LogRow *)
{
    return std::string_view{"user_logs"};
}
YLT_REFL(UserRow, id, uid, username, display_name, role, status, version, force_password_change, temporary_credential_expires_at,
         failed_login_count, locked_until, password_changed_at, disabled_at, created_by, authz_version, last_login_at, created_at,
         updated_at);
REGISTER_CONFLICT_KEY(UserRow, id)
YLT_REFL(NewUser, id, uid, uid_canonical, username, username_canonical, display_name, password_hash, role, status,
         force_password_change, temporary_credential_expires_at, created_by);
YLT_REFL(LogRow, id, actor_user_id, target_user_id, event_name, result, request_id, details_json, created_at);
} // namespace SZX::ADMIN::USER

namespace SZX::ADMIN::INFRA::DETAIL
{
// JSON metadata reports an unbounded width. The ormpp string binding can overflow
// that width on Windows; its blob binding retrieves the actual byte length instead.
struct LogRecord
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    ormpp::blob details_json;
    std::string created_at;
};
YLT_REFL(LogRecord, id, actor_user_id, target_user_id, event_name, result, request_id, details_json, created_at);

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
// Fixed placeholder count for the role allowlist; ormpp binds a compile-time number of parameters.
constexpr std::size_t kRoleSlots = 4;

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

/** Pad the caller's role allowlist to a fixed width by repeating an entry. */
auto roleSlots(const std::vector<std::string> &roles) -> std::array<std::string, kRoleSlots>
{
    std::array<std::string, kRoleSlots> slots;
    for (std::size_t index = 0; index < kRoleSlots; ++index)
        slots[index] = roles[index < roles.size() ? index : 0];
    return slots;
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

auto MySqlUserRepository::findByCanonical(const std::string &uid, const std::string &username)
    -> Result<std::optional<USER::UserRow>>
{
    try
    {
        // Binary equality prevents the accent/case-insensitive collation from reporting
        // a different account as a duplicate of the requested identity.
        auto rows = connection_.query_s<USER::UserRow>("((uid_canonical = ? AND BINARY uid_canonical = ?) OR "
                                                       "(username_canonical = ? AND BINARY username_canonical = ?)) "
                                                       "ORDER BY id LIMIT 1 FOR UPDATE",
                                                       uid, uid, username, username);
        if (connection_.has_error())
            return queryFailed<std::optional<USER::UserRow>>();
        if (rows.empty())
            return Result<std::optional<USER::UserRow>>::Success(std::nullopt);
        return Result<std::optional<USER::UserRow>>::Success(std::move(rows.front()));
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
        const auto roles = roleSlots(filter.roles);
        // Empty filters are neutralized by their own placeholder so the parameter count stays fixed.
        const auto status = filter.status;
        const auto search = filter.search;
        const auto needle = search.empty() ? std::string{} : "%" + escapeLike(search) + "%";
        const auto limit  = static_cast<std::uint64_t>(filter.limit);
        const auto offset = filter.offset;
        const std::string predicate{"role IN (?, ?, ?, ?) AND (? = '' OR status = ?) AND "
                                    "(? = '' OR display_name LIKE ? OR uid LIKE ? OR username LIKE ?)"};

        const auto totals =
            connection_.query_s<std::tuple<std::uint64_t>>("SELECT COUNT(*) FROM users WHERE " + predicate, roles[0], roles[1],
                                                           roles[2], roles[3], status, status, search, needle, needle, needle);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<USER::UserPage>();

        USER::UserPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total > offset)
        {
            auto rows = connection_.query_s<USER::UserRow>(predicate + std::string{orderBy(filter.sort)} + " LIMIT ? OFFSET ?",
                                                           roles[0], roles[1], roles[2], roles[3], status, status, search, needle,
                                                           needle, needle, limit, offset);
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

auto MySqlUserRepository::queryLogs(const USER::LogFilter &filter) -> Result<USER::LogPage>
{
    try
    {
        if (filter.limit == 0)
            return Result<USER::LogPage>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto from   = filter.from;
        const auto to     = filter.to;
        const auto event  = filter.eventName;
        const auto result = filter.result;
        const auto actor  = filter.actorUserId;
        const auto limit  = static_cast<std::uint64_t>(filter.limit);
        const auto offset = filter.offset;
        const std::string predicate{"(? = '' OR created_at >= ?) AND (? = '' OR created_at <= ?) AND "
                                    "(? = '' OR event_name = ?) AND (? = '' OR result = ?) AND "
                                    "(? = '' OR actor_user_id = ?)"};

        const auto totals = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*) FROM user_logs WHERE " + predicate, from, from, to, to, event, event, result, result, actor, actor);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<USER::LogPage>();

        USER::LogPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total > offset)
        {
            auto rows =
                connection_.query_s<LogRecord>("SELECT id, actor_user_id, target_user_id, event_name, result, request_id, "
                                               "CAST(details_json AS BINARY), created_at FROM user_logs WHERE " +
                                                   predicate + " ORDER BY created_at DESC, id ASC LIMIT ? OFFSET ?",
                                               from, from, to, to, event, event, result, result, actor, actor, limit, offset);
            if (connection_.has_error())
                return queryFailed<USER::LogPage>();
            page.items.reserve(rows.size());
            for (auto &row : rows)
                page.items.push_back({.id             = std::move(row.id),
                                      .actor_user_id  = std::move(row.actor_user_id),
                                      .target_user_id = std::move(row.target_user_id),
                                      .event_name     = std::move(row.event_name),
                                      .result         = std::move(row.result),
                                      .request_id     = std::move(row.request_id),
                                      .details_json   = std::string(row.details_json.begin(), row.details_json.end()),
                                      .created_at     = std::move(row.created_at)});
        }
        return Result<USER::LogPage>::Success(std::move(page));
    } catch (...)
    {
        return queryFailed<USER::LogPage>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
