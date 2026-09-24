#pragma once

#include "../user/ActorContext.h"
#include "../user/UserRepository.h"

#include "Result.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::RBAC
{
struct AuthorizedActor final
{
    USER::UserRow user;
};

struct AuthorizedMutation final
{
    USER::UserRow actor;
    USER::UserRow target;
    std::uint64_t activeSuperAdmins{};
};

/**
 * @brief Owns request authentication and user authorization decisions.
 *
 * The current implementation adapts the first-stage Policy matrix. The
 * application-facing contract uses stable permission codes so the policy
 * source can move to the RBAC repositories without changing UserService,
 * AuditService, or BehaviorEventService call sites.
 */
class AuthorizationService final
{
public:
    explicit AuthorizationService(AUTH::LoginService &auth) noexcept;

    [[nodiscard]] auto resolve(const USER::ActorContext &context, USER::UserRepository &repository, bool lock = false)
        -> SZX::COMMON::Result<AuthorizedActor>;

    [[nodiscard]] auto authorizeUserMutation(const USER::ActorContext &context, USER::UserRepository &repository,
                                             std::string_view targetId, std::string_view permissionCode, std::uint32_t version)
        -> SZX::COMMON::Result<AuthorizedMutation>;

    [[nodiscard]] auto allows(std::string_view role, std::string_view permissionCode) const noexcept -> bool;
    [[nodiscard]] auto mayTarget(std::string_view actorRole, std::string_view targetRole) const noexcept -> bool;
    [[nodiscard]] auto validRole(std::string_view role) const noexcept -> bool;
    [[nodiscard]] auto validStatus(std::string_view status) const noexcept -> bool;
    [[nodiscard]] auto capabilities(std::string_view role) const -> SZX::COMMON::Result<std::vector<std::string_view>>;
    /** 返回调用者可以管理的目标角色；角色清单只在授权边界维护。 */
    [[nodiscard]] auto manageableRoles(std::string_view actorRole) const -> std::vector<std::string_view>;

private:
    AUTH::LoginService &auth_;
};
} // namespace SZX::ADMIN::RBAC
