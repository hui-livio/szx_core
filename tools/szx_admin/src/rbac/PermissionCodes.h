#pragma once

#include <string_view>

namespace SZX::ADMIN::RBAC::PERMISSIONS
{
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
} // namespace SZX::ADMIN::RBAC::PERMISSIONS
