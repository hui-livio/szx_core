#include "RoleRepository.h"

#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"

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
