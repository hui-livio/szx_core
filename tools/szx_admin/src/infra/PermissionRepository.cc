#include "PermissionRepository.h"

#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <tuple>

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
struct permission_record final
{
    std::string id;
    std::string code;
    std::string name;
    std::string module;
    std::string enforcement;
    std::string source;
    std::string status;
    bool required_for_super_admin{};
    std::uint32_t version{};
    std::string description;
};
YLT_REFL(permission_record, id, code, name, module, enforcement, source, status, required_for_super_admin, version, description);
inline constexpr auto get_alias_struct_name(permission_record *)
{
    return std::string_view{"permissions"};
}

struct permission_insert final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::string module;
    std::string enforcement;
    std::string source;
    std::string status;
    bool required_for_super_admin{};
};
YLT_REFL(permission_insert, id, code, name, description, module, enforcement, source, status, required_for_super_admin);
inline constexpr auto get_alias_struct_name(permission_insert *)
{
    return std::string_view{"permissions"};
}

struct permission_update final
{
    std::string id;
    std::string name;
    std::string description;
    std::string status;
    std::uint32_t version{};
    std::string updated_by;
    std::optional<std::string> archived_at;
};
YLT_REFL(permission_update, id, name, description, status, version, updated_by, archived_at);
inline constexpr auto get_alias_struct_name(permission_update *)
{
    return std::string_view{"permissions"};
}
REGISTER_CONFLICT_KEY(permission_update, id)

template <typename T>
auto failed() -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
}

auto project(permission_record row) -> RBAC::PermissionRow
{
    return {.id                    = std::move(row.id),
            .code                  = std::move(row.code),
            .name                  = std::move(row.name),
            .module                = std::move(row.module),
            .enforcement           = std::move(row.enforcement),
            .source                = std::move(row.source),
            .status                = std::move(row.status),
            .requiredForSuperAdmin = row.required_for_super_admin,
            .version               = row.version,
            .description           = std::move(row.description)};
}
} // namespace

MySqlPermissionRepository::MySqlPermissionRepository(OrmConnection &connection) noexcept :
    connection_(connection)
{
}

auto MySqlPermissionRepository::findById(std::string_view id, bool lock)
    -> SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>
{
    try
    {
        auto rows = connection_.query_s<permission_record>(lock ? "id = ? FOR UPDATE" : "id = ?", std::string{id});
        if (connection_.has_error())
            return failed<std::optional<RBAC::PermissionRow>>();
        if (rows.size() != 1)
            return SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>::Success(std::nullopt);
        return SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>::Success(project(std::move(rows.front())));
    } catch (...)
    {
        return failed<std::optional<RBAC::PermissionRow>>();
    }
}

auto MySqlPermissionRepository::findByCode(std::string_view code, bool lock)
    -> SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>
{
    try
    {
        auto rows = connection_.query_s<permission_record>(lock ? "code = ? FOR UPDATE" : "code = ?", std::string{code});
        if (connection_.has_error())
            return failed<std::optional<RBAC::PermissionRow>>();
        if (rows.size() != 1)
            return SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>::Success(std::nullopt);
        return SZX::COMMON::Result<std::optional<RBAC::PermissionRow>>::Success(project(std::move(rows.front())));
    } catch (...)
    {
        return failed<std::optional<RBAC::PermissionRow>>();
    }
}

auto MySqlPermissionRepository::list(bool includeArchived) -> SZX::COMMON::Result<std::vector<RBAC::PermissionRow>>
{
    try
    {
        auto rows = connection_.query_s<permission_record>(includeArchived ? "ORDER BY module, code" :
                                                                             "status <> 'archived' ORDER BY module, code");
        if (connection_.has_error())
            return failed<std::vector<RBAC::PermissionRow>>();
        std::vector<RBAC::PermissionRow> result;
        result.reserve(rows.size());
        for (auto &row : rows)
            result.push_back(project(std::move(row)));
        return SZX::COMMON::Result<std::vector<RBAC::PermissionRow>>::Success(std::move(result));
    } catch (...)
    {
        return failed<std::vector<RBAC::PermissionRow>>();
    }
}

auto MySqlPermissionRepository::insert(const RBAC::NewPermission &permission) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.insert(
            permission_insert{permission.id, permission.code, permission.name, permission.description, permission.module,
                              permission.enforcement, permission.source, permission.status, permission.requiredForSuperAdmin});
        return count == 1 && !connection_.has_error() ?
            SZX::COMMON::Result<void>::Success() :
            SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlPermissionRepository::updateProfile(std::string_view id, std::string_view name, std::string_view description,
                                              std::uint32_t version, std::string_view updatedBy) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.update<&permission_update::name, &permission_update::description,
                                              &permission_update::version, &permission_update::updated_by>(
            permission_update{std::string{id}, std::string{name}, std::string{description}, "active", version,
                              std::string{updatedBy}, std::nullopt});
        return count == 1 && !connection_.has_error() ?
            SZX::COMMON::Result<void>::Success() :
            SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlPermissionRepository::updateStatus(std::string_view id, std::string_view status, std::uint32_t version,
                                             std::string_view updatedBy, std::optional<std::string_view> archivedAt)
    -> SZX::COMMON::Result<void>
{
    try
    {
        const auto count = connection_.update<&permission_update::status, &permission_update::version,
                                              &permission_update::updated_by, &permission_update::archived_at>(
            permission_update{std::string{id},
                              {},
                              {},
                              std::string{status},
                              version,
                              std::string{updatedBy},
                              archivedAt ? std::optional<std::string>{std::string{*archivedAt}} : std::nullopt});
        return count == 1 && !connection_.has_error() ?
            SZX::COMMON::Result<void>::Success() :
            SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
    }
}

auto MySqlPermissionRepository::listActiveCodes(std::string_view roleCode) -> SZX::COMMON::Result<std::vector<std::string>>
{
    try
    {
        const auto rows = connection_.query_s<std::tuple<std::string>>(
            "SELECT p.code FROM role_permissions rp JOIN roles r ON r.id = rp.role_id "
            "JOIN permissions p ON p.id = rp.permission_id WHERE r.code = ? AND r.status = 'active' "
            "AND p.status = 'active' ORDER BY p.code",
            std::string{roleCode});
        if (connection_.has_error())
            return failed<std::vector<std::string>>();
        std::vector<std::string> result;
        result.reserve(rows.size());
        for (const auto &row : rows)
            result.push_back(std::get<0>(row));
        return SZX::COMMON::Result<std::vector<std::string>>::Success(std::move(result));
    } catch (...)
    {
        return failed<std::vector<std::string>>();
    }
}

auto MySqlPermissionRepository::roleBindingCount(std::string_view permissionId) -> SZX::COMMON::Result<std::uint64_t>
{
    try
    {
        const auto rows = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*) FROM role_permissions WHERE permission_id = ?", std::string{permissionId});
        if (connection_.has_error() || rows.size() != 1)
            return failed<std::uint64_t>();
        return SZX::COMMON::Result<std::uint64_t>::Success(std::get<0>(rows.front()));
    } catch (...)
    {
        return failed<std::uint64_t>();
    }
}

auto MySqlPermissionRepository::bumpRoleAuthzVersions(std::string_view permissionId) -> SZX::COMMON::Result<void>
{
    try
    {
        return connection_.execute(
                   "UPDATE roles r JOIN role_permissions rp ON rp.role_id = r.id SET r.version = r.version + 1, r.authz_version = r.authz_version + 1 WHERE rp.permission_id = '" +
                   std::string{permissionId} + "'") ?
            SZX::COMMON::Result<void>::Success() :
            failed<void>();
    } catch (...)
    {
        return failed<void>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
