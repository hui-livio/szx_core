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
    auto list(bool includeArchived = false) -> SZX::COMMON::Result<std::vector<RBAC::PermissionRow>> override;
    auto insert(const RBAC::NewPermission &permission) -> SZX::COMMON::Result<void> override;
    auto updateProfile(std::string_view id, std::string_view name, std::string_view description,
                       std::uint32_t version, std::string_view updatedBy) -> SZX::COMMON::Result<void> override;
    auto updateStatus(std::string_view id, std::string_view status, std::uint32_t version,
                      std::string_view updatedBy, std::optional<std::string_view> archivedAt = std::nullopt)
        -> SZX::COMMON::Result<void> override;
    auto listActiveCodes(std::string_view roleCode)
        -> SZX::COMMON::Result<std::vector<std::string>> override;

private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
