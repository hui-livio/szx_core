#pragma once

#include "../rbac/PermissionRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlPermissionRepository final : public RBAC::PermissionRepository
{
public:
    explicit MySqlPermissionRepository(OrmConnection &connection) noexcept;

    auto findById(std::string_view id, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RBAC::PermissionRow>> override;
    auto findByCode(std::string_view code, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RBAC::PermissionRow>> override;
    auto listActiveCodes(std::string_view roleCode)
        -> SZX::COMMON::Result<std::vector<std::string>> override;

private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
