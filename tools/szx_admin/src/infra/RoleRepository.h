#pragma once

#include "../rbac/RoleRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlRoleRepository final : public RBAC::RoleRepository
{
public:
    explicit MySqlRoleRepository(OrmConnection &connection) noexcept;

    auto findById(std::string_view id, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RBAC::RoleRow>> override;
    auto findByCode(std::string_view code, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RBAC::RoleRow>> override;
    auto hasPermission(std::string_view roleCode, std::string_view permissionCode)
        -> SZX::COMMON::Result<bool> override;
    auto canManageRole(std::string_view managerCode, std::string_view targetCode)
        -> SZX::COMMON::Result<bool> override;
    auto manageableRoles(std::string_view managerCode)
        -> SZX::COMMON::Result<std::vector<std::string>> override;

private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
