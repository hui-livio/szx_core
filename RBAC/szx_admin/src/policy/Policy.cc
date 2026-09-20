#include "SzxAdmin/Policy/Policy.h"

#include <algorithm>
#include <array>

namespace SZX::ADMIN::POLICY
{
namespace
{
constexpr std::array<std::string_view, 4> kRoles{"operator", "engineer", "admin", "super_admin"};
constexpr std::array<std::string_view, 3> kStatuses{"active", "disabled", "locked"};

// Indexed by Capability; keep in sync with the enumeration order.
constexpr std::array<std::string_view, 11> kCodes{
    "business.read", "business.operate", "engineering.configure", "user.read",        "user.create", "user.update",
    "user.status",   "user.unlock",      "user.password.reset",   "user.role.assign", "audit.read",
};

constexpr auto bit(Capability capability) noexcept -> std::uint32_t
{
    return std::uint32_t{1} << static_cast<unsigned>(capability);
}

// Capability matrix from the backend design document; roles are strictly cumulative.
constexpr std::uint32_t kOperatorMask = bit(Capability::BusinessRead) | bit(Capability::BusinessOperate);
constexpr std::uint32_t kEngineerMask = kOperatorMask | bit(Capability::EngineeringConfigure);
constexpr std::uint32_t kAdminMask    = kEngineerMask | bit(Capability::UserRead) | bit(Capability::UserCreate) |
    bit(Capability::UserUpdate) | bit(Capability::UserStatus) | bit(Capability::UserUnlock) | bit(Capability::UserPasswordReset) |
    bit(Capability::AuditRead);
constexpr std::uint32_t kSuperAdminMask = kAdminMask | bit(Capability::UserRoleAssign);

constexpr std::array<std::uint32_t, 4> kMasks{kOperatorMask, kEngineerMask, kAdminMask, kSuperAdminMask};

auto roleIndex(std::string_view role) noexcept -> std::size_t
{
    const auto found = std::find(kRoles.begin(), kRoles.end(), role);
    return found == kRoles.end() ? kRoles.size() : static_cast<std::size_t>(found - kRoles.begin());
}

auto capabilityIndex(Capability capability) noexcept -> std::size_t
{
    const auto index = static_cast<std::size_t>(static_cast<int>(capability));
    return static_cast<int>(capability) < 0 || index >= kCodes.size() ? kCodes.size() : index;
}
} // namespace

auto validRole(std::string_view role) noexcept -> bool
{
    return roleIndex(role) != kRoles.size();
}

auto validStatus(std::string_view status) noexcept -> bool
{
    return std::find(kStatuses.begin(), kStatuses.end(), status) != kStatuses.end();
}

auto capabilityCode(Capability capability) noexcept -> std::string_view
{
    const auto index = capabilityIndex(capability);
    return index == kCodes.size() ? std::string_view{} : kCodes[index];
}

auto allows(std::string_view role, Capability capability) noexcept -> bool
{
    const auto role_index       = roleIndex(role);
    const auto capability_index = capabilityIndex(capability);
    if (role_index == kRoles.size() || capability_index == kCodes.size())
        return false;
    return (kMasks[role_index] & bit(capability)) != 0;
}

auto capabilities(std::string_view role) -> std::vector<std::string_view>
{
    std::vector<std::string_view> result;
    const auto index = roleIndex(role);
    if (index == kRoles.size())
        return result;
    result.reserve(kCodes.size());
    for (std::size_t position = 0; position < kCodes.size(); ++position)
        if ((kMasks[index] & (std::uint32_t{1} << position)) != 0)
            result.push_back(kCodes[position]);
    return result;
}

auto mayTarget(std::string_view actorRole, std::string_view targetRole) noexcept -> bool
{
    if (!validRole(targetRole))
        return false;
    if (actorRole == "super_admin")
        return true;
    // Administrators never read or write administrator/super-admin records.
    return actorRole == "admin" && (targetRole == "operator" || targetRole == "engineer");
}
} // namespace SZX::ADMIN::POLICY
