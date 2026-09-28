#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::RBAC
{
struct PermissionRow final
{
    std::string id;
    std::string code;
    std::string name;
    std::string module;
    std::string enforcement;
    std::string source;
    std::string status;
    bool requiredForSuperAdmin{};
    std::uint32_t version{};
};

struct NewPermission final
{
    std::string id;
    std::string code;
    std::string name;
    std::string description;
    std::string module;
    std::string enforcement{"client"};
    std::string source{"custom"};
    std::string status{"active"};
    bool requiredForSuperAdmin{};
};

/** Runtime permission catalog reads; write behavior belongs to PermissionService. */
class PermissionRepository
{
public:
    virtual ~PermissionRepository() = default;

    virtual auto findById(std::string_view id, bool lock = false)
        -> SZX::COMMON::Result<std::optional<PermissionRow>> = 0;
    virtual auto findByCode(std::string_view code, bool lock = false)
        -> SZX::COMMON::Result<std::optional<PermissionRow>> = 0;
    virtual auto list(bool includeArchived = false) -> SZX::COMMON::Result<std::vector<PermissionRow>> = 0;
    virtual auto insert(const NewPermission &permission) -> SZX::COMMON::Result<void> = 0;
    virtual auto updateProfile(std::string_view id, std::string_view name, std::string_view description,
                               std::uint32_t version, std::string_view updatedBy) -> SZX::COMMON::Result<void> = 0;
    virtual auto updateStatus(std::string_view id, std::string_view status, std::uint32_t version,
                              std::string_view updatedBy, std::optional<std::string_view> archivedAt = std::nullopt)
        -> SZX::COMMON::Result<void> = 0;
    virtual auto listActiveCodes(std::string_view roleCode)
        -> SZX::COMMON::Result<std::vector<std::string>> = 0;
    virtual auto bumpRoleAuthzVersions(std::string_view) -> SZX::COMMON::Result<void>
    { return SZX::COMMON::Result<void>::Success(); }
};
} // namespace SZX::ADMIN::RBAC
