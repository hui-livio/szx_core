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
};
YLT_REFL(permission_record, id, code, name, module, enforcement, source, status, required_for_super_admin, version);

template <typename T>
auto failed() -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
}

auto project(permission_record row) -> RBAC::PermissionRow
{
    return {.id                   = std::move(row.id),
            .code                 = std::move(row.code),
            .name                 = std::move(row.name),
            .module               = std::move(row.module),
            .enforcement          = std::move(row.enforcement),
            .source               = std::move(row.source),
            .status               = std::move(row.status),
            .requiredForSuperAdmin = row.required_for_super_admin,
            .version              = row.version};
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
    }
    catch (...)
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
    }
    catch (...)
    {
        return failed<std::optional<RBAC::PermissionRow>>();
    }
}

auto MySqlPermissionRepository::listActiveCodes(std::string_view roleCode)
    -> SZX::COMMON::Result<std::vector<std::string>>
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
    }
    catch (...)
    {
        return failed<std::vector<std::string>>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
