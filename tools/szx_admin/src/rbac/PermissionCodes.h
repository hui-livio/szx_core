#pragma once

#include <array>
#include <span>
#include <string_view>

namespace SZX::ADMIN::RBAC::PERMISSIONS
{
inline constexpr std::string_view SystemMetricsRead{"system.metrics.read"};
inline constexpr std::string_view BusinessRead{"business.read"};
inline constexpr std::string_view BusinessOperate{"business.operate"};
inline constexpr std::string_view EngineeringConfigure{"engineering.configure"};
inline constexpr std::string_view UserRead{"user.read"};
inline constexpr std::string_view UserCreate{"user.create"};
inline constexpr std::string_view UserUpdate{"user.update"};
inline constexpr std::string_view UserStatus{"user.status"};
inline constexpr std::string_view UserUnlock{"user.unlock"};
inline constexpr std::string_view UserPasswordReset{"user.password.reset"};
inline constexpr std::string_view UserRoleAssign{"user.role.assign"};
inline constexpr std::string_view AuditRead{"audit.read"};
inline constexpr std::string_view BehaviorRead{"behavior.read"};
inline constexpr std::string_view RoleRead{"role.read"};
inline constexpr std::string_view RoleCreate{"role.create"};
inline constexpr std::string_view RoleUpdate{"role.update"};
inline constexpr std::string_view RoleStatus{"role.status"};
inline constexpr std::string_view RoleArchive{"role.archive"};
inline constexpr std::string_view RolePermissionAssign{"role.permission.assign"};
inline constexpr std::string_view RoleScopeAssign{"role.scope.assign"};
inline constexpr std::string_view PermissionRead{"permission.read"};
inline constexpr std::string_view PermissionCreate{"permission.create"};
inline constexpr std::string_view PermissionUpdate{"permission.update"};
inline constexpr std::string_view PermissionStatus{"permission.status"};
inline constexpr std::string_view PermissionArchive{"permission.archive"};

/**
 * @brief Code metadata compiled into the application boundary.
 *
 * The database migration seeds the same server-side catalog. Keeping this
 * registry independent from roles means adding a role never requires a code
 * change, while a missing server permission can fail readiness closed.
 */
struct Definition final
{
    std::string_view code;
    std::string_view module;
    bool requiredForSuperAdmin{};
};

inline constexpr std::array<Definition, 25> Definitions{
    Definition{SystemMetricsRead, "system", true},
    Definition{BusinessRead, "business", true},
    Definition{BusinessOperate, "business", true},
    Definition{EngineeringConfigure, "engineering", true},
    Definition{UserRead, "user", true},
    Definition{UserCreate, "user", true},
    Definition{UserUpdate, "user", true},
    Definition{UserStatus, "user", true},
    Definition{UserUnlock, "user", true},
    Definition{UserPasswordReset, "user", true},
    Definition{UserRoleAssign, "user", true},
    Definition{AuditRead, "audit", true},
    Definition{BehaviorRead, "behavior", true},
    Definition{RoleRead, "role", true},
    Definition{RoleCreate, "role", true},
    Definition{RoleUpdate, "role", true},
    Definition{RoleStatus, "role", true},
    Definition{RoleArchive, "role", true},
    Definition{RolePermissionAssign, "role", true},
    Definition{RoleScopeAssign, "role", true},
    Definition{PermissionRead, "permission", true},
    Definition{PermissionCreate, "permission", true},
    Definition{PermissionUpdate, "permission", true},
    Definition{PermissionStatus, "permission", true},
    Definition{PermissionArchive, "permission", true},
};

[[nodiscard]] inline auto definitions() noexcept -> std::span<const Definition>
{
    return Definitions;
}
} // namespace SZX::ADMIN::RBAC::PERMISSIONS
