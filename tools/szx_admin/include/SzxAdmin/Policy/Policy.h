#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

/**
 * @file Policy.h
 * @brief 固定角色能力矩阵。矩阵在编译期固化，运行时不可编辑。
 */
namespace SZX::ADMIN::POLICY
{
/** @brief 固定能力码。枚举顺序即 /client/me/permissions 的输出顺序。 */
enum class Capability : int
{
    BusinessRead,         ///< business.read
    BusinessOperate,      ///< business.operate
    EngineeringConfigure, ///< engineering.configure
    UserRead,             ///< user.read
    UserCreate,           ///< user.create
    UserUpdate,           ///< user.update
    UserStatus,           ///< user.status
    UserUnlock,           ///< user.unlock
    UserPasswordReset,    ///< user.password.reset
    UserRoleAssign,       ///< user.role.assign
    AuditRead,            ///< audit.read
    BehaviorRead,         ///< behavior.read
};

/** 四个固定角色之一。 */
[[nodiscard]] auto validRole(std::string_view role) noexcept -> bool;
/** active/disabled/locked 之一。 */
[[nodiscard]] auto validStatus(std::string_view status) noexcept -> bool;
/** 稳定的能力码字符串；未知枚举返回空视图。 */
[[nodiscard]] auto capabilityCode(Capability capability) noexcept -> std::string_view;
/** 角色是否具备该能力；未知角色一律返回 false。 */
[[nodiscard]] auto allows(std::string_view role, Capability capability) noexcept -> bool;
/** 按枚举顺序返回角色的能力码；未知角色返回空列表。 */
[[nodiscard]] auto capabilities(std::string_view role) -> std::vector<std::string_view>;
/**
 * @brief 目标角色限制：管理员只能管理 operator/engineer，超管可管理四类角色。
 *
 * 该限制与能力码正交，调用方必须同时校验 allows() 和 mayTarget()。
 */
[[nodiscard]] auto mayTarget(std::string_view actorRole, std::string_view targetRole) noexcept -> bool;
} // namespace SZX::ADMIN::POLICY
