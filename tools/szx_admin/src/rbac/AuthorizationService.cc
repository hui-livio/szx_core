#include "AuthorizationService.h"
#include "PermissionCodes.h"

#include "../auth/LoginService.h"
#include "PermissionRepository.h"
#include "RoleRepository.h"
#include "../user/UserValidation.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Policy/Policy.h"

#include <array>
#include <optional>

namespace SZX::ADMIN::RBAC
{
namespace
{
using INFRA::ErrorCode;
using POLICY::Capability;
using SZX::COMMON::Result;

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto capabilityFor(std::string_view code) noexcept -> std::optional<Capability>
{
    using namespace PERMISSIONS;
    if (code == BusinessRead)
        return Capability::BusinessRead;
    if (code == BusinessOperate)
        return Capability::BusinessOperate;
    if (code == EngineeringConfigure)
        return Capability::EngineeringConfigure;
    if (code == UserRead)
        return Capability::UserRead;
    if (code == UserCreate)
        return Capability::UserCreate;
    if (code == UserUpdate)
        return Capability::UserUpdate;
    if (code == UserStatus)
        return Capability::UserStatus;
    if (code == UserUnlock)
        return Capability::UserUnlock;
    if (code == UserPasswordReset)
        return Capability::UserPasswordReset;
    if (code == UserRoleAssign)
        return Capability::UserRoleAssign;
    if (code == AuditRead)
        return Capability::AuditRead;
    if (code == BehaviorRead)
        return Capability::BehaviorRead;
    return std::nullopt;
}

constexpr std::array<std::string_view, 4> kSupportedRoles{"operator", "engineer", "admin", "super_admin"};
} // namespace

AuthorizationService::AuthorizationService(AUTH::LoginService &auth) noexcept :
    auth_(auth)
{
}

auto AuthorizationService::resolve(const USER::ActorContext &context, USER::UserRepository &repository, bool lock,
                                   RoleRepository *roles)
    -> Result<AuthorizedActor>
{
    try
    {
        const auto claims = auth_.verifyAccess(context.accessToken);
        if (!claims.ok)
            return Result<AuthorizedActor>::Failure(claims.error, {});
        if (!USER::validUuid(claims.value.subject))
            return failed<AuthorizedActor>(ErrorCode::AuthRequired);
        const auto found = repository.findById(claims.value.subject, lock);
        if (!found.ok)
            return Result<AuthorizedActor>::Failure(found.error, {});
        if (!found.value || found.value->status != "active" || found.value->authz_version <= 0 ||
            static_cast<std::uint64_t>(found.value->authz_version) != claims.value.authzVersion)
            return failed<AuthorizedActor>(ErrorCode::AuthRequired);
        if (found.value->role == "super_admin" && found.value->force_password_change)
            return failed<AuthorizedActor>(ErrorCode::Forbidden);
        if (roles)
        {
            const auto currentRole = roles->findById(claims.value.roleId);
            if (!currentRole.ok)
                return Result<AuthorizedActor>::Failure(currentRole.error, {});
            if (!currentRole.value || currentRole.value->status != "active" || currentRole.value->code != found.value->role ||
                currentRole.value->authzVersion != claims.value.roleAuthzVersion)
                return failed<AuthorizedActor>(ErrorCode::AuthRequired);
        }
        return Result<AuthorizedActor>::Success({.user = *found.value});
    }
    catch (...)
    {
        return failed<AuthorizedActor>(ErrorCode::InternalFailure);
    }
}

auto AuthorizationService::authorizeUserMutation(const USER::ActorContext &context, USER::UserRepository &repository,
                                                  std::string_view targetId, std::string_view permissionCode,
                                                  std::uint32_t version, RoleRepository *roles) -> Result<AuthorizedMutation>
{
    try
    {
        const auto claims = auth_.verifyAccess(context.accessToken);
        if (!claims.ok)
            return Result<AuthorizedMutation>::Failure(claims.error, {});
        if (!USER::validUuid(claims.value.subject) || !USER::validUuid(targetId))
            return failed<AuthorizedMutation>(ErrorCode::AuthRequired);
        const auto locked = repository.lockForMutation(claims.value.subject, std::string{targetId});
        if (!locked.ok)
            return Result<AuthorizedMutation>::Failure(locked.error, {});
        const auto &actor = locked.value.actor;
        if (!actor || actor->status != "active" || actor->authz_version <= 0 ||
            static_cast<std::uint64_t>(actor->authz_version) != claims.value.authzVersion)
            return failed<AuthorizedMutation>(ErrorCode::AuthRequired);
        if (actor->role == "super_admin" && actor->force_password_change)
            return failed<AuthorizedMutation>(ErrorCode::Forbidden);
        if (roles)
        {
            const auto currentRole = roles->findById(claims.value.roleId);
            if (!currentRole.ok)
                return Result<AuthorizedMutation>::Failure(currentRole.error, {});
            if (!currentRole.value || currentRole.value->status != "active" || currentRole.value->code != actor->role ||
                currentRole.value->authzVersion != claims.value.roleAuthzVersion)
                return failed<AuthorizedMutation>(ErrorCode::AuthRequired);
            const auto permitted = roles->hasPermission(actor->role, permissionCode);
            if (!permitted.ok)
                return Result<AuthorizedMutation>::Failure(permitted.error, {});
            if (!permitted.value)
                return failed<AuthorizedMutation>(ErrorCode::Forbidden);
        }
        else if (!allows(actor->role, permissionCode))
            return failed<AuthorizedMutation>(ErrorCode::Forbidden);
        if (!locked.value.target)
            return failed<AuthorizedMutation>(ErrorCode::ResourceNotFound);
        if (roles)
        {
            const auto manageable = roles->canManageRole(actor->role, locked.value.target->role);
            if (!manageable.ok)
                return Result<AuthorizedMutation>::Failure(manageable.error, {});
            if (!manageable.value)
                return failed<AuthorizedMutation>(ErrorCode::Forbidden);
        }
        else if (!mayTarget(actor->role, locked.value.target->role))
            return failed<AuthorizedMutation>(ErrorCode::Forbidden);
        if (locked.value.target->version != static_cast<int>(version))
            return failed<AuthorizedMutation>(ErrorCode::ConflictVersion);
        return Result<AuthorizedMutation>::Success({.actor = *actor,
                                                    .target = *locked.value.target,
                                                    .activeSuperAdmins = locked.value.activeSuperAdmins});
    }
    catch (...)
    {
        return failed<AuthorizedMutation>(ErrorCode::InternalFailure);
    }
}

auto AuthorizationService::allows(std::string_view role, std::string_view permissionCode) const noexcept -> bool
{
    const auto capability = capabilityFor(permissionCode);
    return capability && POLICY::allows(role, *capability);
}

auto AuthorizationService::allows(RoleRepository &roles, std::string_view role, std::string_view permissionCode) const
    -> Result<bool>
{
    try
    {
        const auto currentRole = roles.findByCode(role);
        if (!currentRole.ok)
            return Result<bool>::Failure(currentRole.error, {});
        if (!currentRole.value || currentRole.value->status != "active")
            return Result<bool>::Success(false);
        return roles.hasPermission(role, permissionCode);
    }
    catch (...)
    {
        return failed<bool>(ErrorCode::InternalFailure);
    }
}

auto AuthorizationService::mayTarget(std::string_view actorRole, std::string_view targetRole) const noexcept -> bool
{
    return POLICY::mayTarget(actorRole, targetRole);
}

auto AuthorizationService::validRole(std::string_view role) const noexcept -> bool
{
    return POLICY::validRole(role);
}

auto AuthorizationService::validStatus(std::string_view status) const noexcept -> bool
{
    return POLICY::validStatus(status);
}

auto AuthorizationService::capabilities(std::string_view role) const -> Result<std::vector<std::string_view>>
{
    try
    {
        return Result<std::vector<std::string_view>>::Success(POLICY::capabilities(role));
    }
    catch (...)
    {
        return failed<std::vector<std::string_view>>(ErrorCode::InternalFailure);
    }
}

auto AuthorizationService::capabilities(PermissionRepository &permissions, std::string_view role) const
    -> Result<std::vector<std::string>>
{
    try
    {
        return permissions.listActiveCodes(role);
    }
    catch (...)
    {
        return failed<std::vector<std::string>>(ErrorCode::InternalFailure);
    }
}

auto AuthorizationService::manageableRoles(std::string_view actorRole) const -> std::vector<std::string_view>
{
    std::vector<std::string_view> result;
    for (const auto role : kSupportedRoles)
        if (mayTarget(actorRole, role))
            result.push_back(role);
    return result;
}

auto AuthorizationService::manageableRoles(RoleRepository &roles, std::string_view actorRole) const
    -> Result<std::vector<std::string>>
{
    try
    {
        return roles.manageableRoles(actorRole);
    }
    catch (...)
    {
        return failed<std::vector<std::string>>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::RBAC
