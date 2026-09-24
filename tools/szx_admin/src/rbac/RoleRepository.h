#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::RBAC
{
struct RoleRow final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::optional<std::string> systemKey;
    std::string status;
    std::uint32_t version{};
    std::uint32_t authzVersion{};
};

struct NewRole final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::string status{"active"};
    std::uint32_t version{1};
    std::uint32_t authzVersion{1};
    std::optional<std::string> createdBy;
};

/** Runtime role and management-scope reads used by the authorization boundary. */
class RoleRepository
{
public:
    virtual ~RoleRepository() = default;

    virtual auto findById(std::string_view id, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RoleRow>> = 0;
    virtual auto findByCode(std::string_view code, bool lock = false)
        -> SZX::COMMON::Result<std::optional<RoleRow>> = 0;
    virtual auto list(bool includeArchived = false) -> SZX::COMMON::Result<std::vector<RoleRow>> = 0;
    virtual auto insert(const NewRole &role) -> SZX::COMMON::Result<void> = 0;
    virtual auto updateProfile(std::string_view id, std::string_view name, std::string_view description,
                               std::uint32_t version, std::string_view updatedBy) -> SZX::COMMON::Result<void> = 0;
    virtual auto updateStatus(std::string_view id, std::string_view status, std::uint32_t version,
                              std::uint32_t authzVersion, std::string_view updatedBy,
                              std::optional<std::string_view> archivedAt = std::nullopt)
        -> SZX::COMMON::Result<void> = 0;
    virtual auto hasPermission(std::string_view roleCode, std::string_view permissionCode)
        -> SZX::COMMON::Result<bool> = 0;
    virtual auto canManageRole(std::string_view managerCode, std::string_view targetCode)
        -> SZX::COMMON::Result<bool> = 0;
    virtual auto manageableRoles(std::string_view managerCode)
        -> SZX::COMMON::Result<std::vector<std::string>> = 0;
};
} // namespace SZX::ADMIN::RBAC
