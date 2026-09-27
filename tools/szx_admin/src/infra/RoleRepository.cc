#include "RoleRepository.h"

#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "UUID.hpp"

#include <tuple>

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
struct role_record final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::optional<std::string> system_key;
    std::string status;
    std::uint32_t version{};
    std::uint32_t authz_version{};
};
YLT_REFL(role_record, id, code, name, description, system_key, status, version, authz_version);
inline constexpr auto get_alias_struct_name(role_record *)
{
    return std::string_view{"roles"};
}

struct role_insert final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::optional<std::string> system_key;
    std::string status;
    std::uint32_t version{};
    std::uint32_t authz_version{};
    std::optional<std::string> created_by;
};
YLT_REFL(role_insert, id, code, name, description, system_key, status, version, authz_version, created_by);
inline constexpr auto get_alias_struct_name(role_insert *)
{
    return std::string_view{"roles"};
}

struct role_update final
{
    std::string id;
    std::string name;
    std::string description;
    std::string status;
    std::uint32_t version{};
    std::uint32_t authz_version{};
    std::string updated_by;
    std::optional<std::string> archived_at;
};
YLT_REFL(role_update, id, name, description, status, version, authz_version, updated_by, archived_at);
REGISTER_CONFLICT_KEY(role_update, id)
inline constexpr auto get_alias_struct_name(role_update *)
{
    return std::string_view{"roles"};
}

struct role_permission_insert final
{
    std::string role_id;
    std::string permission_id;
    std::optional<std::string> assigned_by;
};
YLT_REFL(role_permission_insert, role_id, permission_id, assigned_by);
inline constexpr auto get_alias_struct_name(role_permission_insert *)
{
    return std::string_view{"role_permissions"};
}

struct role_scope_insert final
{
    std::string manager_role_id;
    std::string target_role_id;
    std::optional<std::string> assigned_by;
};
YLT_REFL(role_scope_insert, manager_role_id, target_role_id, assigned_by);
inline constexpr auto get_alias_struct_name(role_scope_insert *)
{
    return std::string_view{"role_manage_scopes"};
}

template <typename T>
auto failed() -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
}

auto project(role_record row) -> RBAC::RoleRow
{
    return {.id           = std::move(row.id),
            .code         = std::move(row.code),
            .name         = std::move(row.name),
            .description  = std::move(row.description),
            .systemKey    = std::move(row.system_key),
            .status       = std::move(row.status),
            .version      = row.version,
            .authzVersion = row.authz_version};
}
} // namespace

MySqlRoleRepository::MySqlRoleRepository(OrmConnection &connection) noexcept :
    connection_(connection)
{
}

auto MySqlRoleRepository::findById(std::string_view id, bool lock)
    -> SZX::COMMON::Result<std::optional<RBAC::RoleRow>>
{
    try
    {
        auto rows = connection_.query_s<role_record>(lock ? "id = ? FOR UPDATE" : "id = ?", std::string{id});
        if (connection_.has_error())
            return failed<std::optional<RBAC::RoleRow>>();
        if (rows.size() != 1)
            return SZX::COMMON::Result<std::optional<RBAC::RoleRow>>::Success(std::nullopt);
        return SZX::COMMON::Result<std::optional<RBAC::RoleRow>>::Success(project(std::move(rows.front())));
    }
    catch (...)
    {
        return failed<std::optional<RBAC::RoleRow>>();
    }
}

auto MySqlRoleRepository::findByCode(std::string_view code, bool lock)
    -> SZX::COMMON::Result<std::optional<RBAC::RoleRow>>
{
    try
    {
        auto rows = connection_.query_s<role_record>(lock ? "code = ? FOR UPDATE" : "code = ?", std::string{code});
        if (connection_.has_error())
            return failed<std::optional<RBAC::RoleRow>>();
        if (rows.size() != 1)
            return SZX::COMMON::Result<std::optional<RBAC::RoleRow>>::Success(std::nullopt);
        return SZX::COMMON::Result<std::optional<RBAC::RoleRow>>::Success(project(std::move(rows.front())));
    }
    catch (...)
    {
        return failed<std::optional<RBAC::RoleRow>>();
    }
}

auto MySqlRoleRepository::list(bool includeArchived) -> SZX::COMMON::Result<std::vector<RBAC::RoleRow>>
{
    try
    {
        auto rows = connection_.query_s<role_record>(includeArchived ? "ORDER BY name, code" : "status <> 'archived' ORDER BY name, code");
        if (connection_.has_error())
            return failed<std::vector<RBAC::RoleRow>>();
        std::vector<RBAC::RoleRow> result;
        result.reserve(rows.size());
        for (auto &row : rows)
            result.push_back(project(std::move(row)));
        return SZX::COMMON::Result<std::vector<RBAC::RoleRow>>::Success(std::move(result));
    }
    catch (...)
    {
        return failed<std::vector<RBAC::RoleRow>>();
    }
}

auto MySqlRoleRepository::insert(const RBAC::NewRole &role) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.insert(role_insert{role.id, role.code, role.name, role.description, std::nullopt,
                                                          role.status, role.version, role.authzVersion, role.createdBy});
        return count == 1 && !connection_.has_error() ? SZX::COMMON::Result<void>::Success() :
                                                        SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
    catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlRoleRepository::updateProfile(std::string_view id, std::string_view name, std::string_view description,
                                        std::uint32_t version, std::string_view updatedBy) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.update<&role_update::name, &role_update::description, &role_update::version,
                                              &role_update::updated_by>(role_update{std::string{id}, std::string{name},
                                                                                        std::string{description}, "active", version,
                                                                                        1, std::string{updatedBy}, std::nullopt});
        return count == 1 && !connection_.has_error() ? SZX::COMMON::Result<void>::Success() :
                                                        SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
    catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlRoleRepository::updateStatus(std::string_view id, std::string_view status, std::uint32_t version,
                                       std::uint32_t authzVersion, std::string_view updatedBy,
                                       std::optional<std::string_view> archivedAt) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.update<&role_update::status, &role_update::version, &role_update::authz_version,
                                              &role_update::updated_by, &role_update::archived_at>(
            role_update{std::string{id}, {}, {}, std::string{status}, version, authzVersion, std::string{updatedBy},
                       archivedAt ? std::optional<std::string>{std::string{*archivedAt}} : std::nullopt});
        return count == 1 && !connection_.has_error() ? SZX::COMMON::Result<void>::Success() :
                                                        SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
    catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto validUuidList(std::string_view roleId, const std::vector<std::string> &ids, std::string_view assignedBy) noexcept -> bool
{
    if (!SZX::COMMON::Uuid::parse(roleId) || !SZX::COMMON::Uuid::parse(assignedBy))
        return false;
    for (const auto &id : ids)
        if (!SZX::COMMON::Uuid::parse(id))
            return false;
    return true;
}

auto MySqlRoleRepository::replacePermissions(std::string_view roleId, const std::vector<std::string> &permissionIds,
                                             std::string_view assignedBy) -> SZX::COMMON::Result<void>
{
    try
    {
        if (!validUuidList(roleId, permissionIds, assignedBy))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::ValidationError), {});
        const auto deleted = connection_.execute("DELETE FROM role_permissions WHERE role_id = '" + std::string{roleId} + "'");
        if (!deleted)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        std::vector<role_permission_insert> rows;
        rows.reserve(permissionIds.size());
        for (const auto &permissionId : permissionIds)
            rows.push_back({std::string{roleId}, permissionId, std::string{assignedBy}});
        if (!rows.empty() && connection_.insert(rows) != static_cast<int>(rows.size()))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        return SZX::COMMON::Result<void>::Success();
    }
    catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlRoleRepository::replaceManageScopes(std::string_view roleId, const std::vector<std::string> &targetRoleIds,
                                              std::string_view assignedBy) -> SZX::COMMON::Result<void>
{
    try
    {
        if (!validUuidList(roleId, targetRoleIds, assignedBy))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::ValidationError), {});
        const auto deleted = connection_.execute("DELETE FROM role_manage_scopes WHERE manager_role_id = '" + std::string{roleId} + "'");
        if (!deleted)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        std::vector<role_scope_insert> rows;
        rows.reserve(targetRoleIds.size());
        for (const auto &targetId : targetRoleIds)
            rows.push_back({std::string{roleId}, targetId, std::string{assignedBy}});
        if (!rows.empty() && connection_.insert(rows) != static_cast<int>(rows.size()))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        return SZX::COMMON::Result<void>::Success();
    }
    catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlRoleRepository::permissionIds(std::string_view roleId) -> SZX::COMMON::Result<std::vector<std::string>>
{
    try
    {
        if (!SZX::COMMON::Uuid::parse(roleId))
            return SZX::COMMON::Result<std::vector<std::string>>::Failure(static_cast<int>(ErrorCode::ValidationError), {});
        const auto rows = connection_.query_s<std::tuple<std::string>>(
            "SELECT permission_id FROM role_permissions WHERE role_id = ? ORDER BY permission_id", std::string{roleId});
        if (connection_.has_error())
            return failed<std::vector<std::string>>();
        std::vector<std::string> result;
        result.reserve(rows.size());
        for (const auto &row : rows)
            result.push_back(std::get<0>(row));
        return SZX::COMMON::Result<std::vector<std::string>>::Success(std::move(result));
    }
    catch (...)
    {
        return failed<std::vector<std::string>>();
    }
}

auto MySqlRoleRepository::manageScopeIds(std::string_view roleId) -> SZX::COMMON::Result<std::vector<std::string>>
{
    try
    {
        if (!SZX::COMMON::Uuid::parse(roleId))
            return SZX::COMMON::Result<std::vector<std::string>>::Failure(static_cast<int>(ErrorCode::ValidationError), {});
        const auto rows = connection_.query_s<std::tuple<std::string>>(
            "SELECT target_role_id FROM role_manage_scopes WHERE manager_role_id = ? ORDER BY target_role_id",
            std::string{roleId});
        if (connection_.has_error())
            return failed<std::vector<std::string>>();
        std::vector<std::string> result;
        result.reserve(rows.size());
        for (const auto &row : rows)
            result.push_back(std::get<0>(row));
        return SZX::COMMON::Result<std::vector<std::string>>::Success(std::move(result));
    }
    catch (...)
    {
        return failed<std::vector<std::string>>();
    }
}

auto MySqlRoleRepository::hasPermission(std::string_view roleCode, std::string_view permissionCode)
    -> SZX::COMMON::Result<bool>
{
    try
    {
        const auto rows = connection_.query_s<std::tuple<int>>(
            "SELECT EXISTS(SELECT 1 FROM role_permissions rp JOIN roles r ON r.id = rp.role_id "
            "JOIN permissions p ON p.id = rp.permission_id WHERE r.code = ? AND r.status = 'active' "
            "AND p.code = ? AND p.status = 'active')",
            std::string{roleCode}, std::string{permissionCode});
        if (connection_.has_error() || rows.size() != 1)
            return failed<bool>();
        return SZX::COMMON::Result<bool>::Success(std::get<0>(rows.front()) != 0);
    }
    catch (...)
    {
        return failed<bool>();
    }
}

auto MySqlRoleRepository::canManageRole(std::string_view managerCode, std::string_view targetCode)
    -> SZX::COMMON::Result<bool>
{
    try
    {
        const auto rows = connection_.query_s<std::tuple<int>>(
            "SELECT EXISTS(SELECT 1 FROM role_manage_scopes scope "
            "JOIN roles manager ON manager.id = scope.manager_role_id "
            "JOIN roles target ON target.id = scope.target_role_id "
            "WHERE manager.code = ? AND manager.status = 'active' AND target.code = ? AND target.status <> 'archived')",
            std::string{managerCode}, std::string{targetCode});
        if (connection_.has_error() || rows.size() != 1)
            return failed<bool>();
        return SZX::COMMON::Result<bool>::Success(std::get<0>(rows.front()) != 0);
    }
    catch (...)
    {
        return failed<bool>();
    }
}

auto MySqlRoleRepository::manageableRoles(std::string_view managerCode)
    -> SZX::COMMON::Result<std::vector<std::string>>
{
    try
    {
        const auto rows = connection_.query_s<std::tuple<std::string>>(
            "SELECT target.code FROM role_manage_scopes scope "
            "JOIN roles manager ON manager.id = scope.manager_role_id "
            "JOIN roles target ON target.id = scope.target_role_id "
            "WHERE manager.code = ? AND manager.status = 'active' AND target.status <> 'archived' ORDER BY target.code",
            std::string{managerCode});
        if (connection_.has_error())
            return failed<std::vector<std::string>>();
        std::vector<std::string> result;
        result.reserve(rows.size());
        for (const auto &row : rows)
            result.push_back(std::get<0>(row));
        return SZX::COMMON::Result<std::vector<std::string>>::Success(std::move(result));
    }
    catch (...)
    {
        return failed<std::vector<std::string>>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
