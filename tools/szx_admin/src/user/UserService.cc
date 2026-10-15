#include "UserService.h"
#include "../auth/LoginService.h"
#include "../rbac/PermissionCodes.h"
#include "../rbac/PermissionRepository.h"
#include "../rbac/RoleRepository.h"
#include "UserValidation.h"

#include "SzxAdmin/Infra/ErrorCode.h"

#include <algorithm>
#include <utility>

namespace SZX::ADMIN::USER
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

} // namespace

UserService::UserService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    auth_(auth),
    authorization_(auth)
{
}

auto UserService::resolve(const ActorContext &context, UserRepository &repository, bool lock, RBAC::RoleRepository *roles)
    -> Result<RBAC::AuthorizedActor>
{
    return authorization_.resolve(context, repository, lock, roles);
}

auto UserService::authorize(const ActorContext &context, UserRepository &repository, const std::string &targetId,
                            std::string_view permissionCode, std::uint32_t version, RBAC::RoleRepository *roles)
    -> Result<RBAC::AuthorizedMutation>
{
    return authorization_.authorizeUserMutation(context, repository, targetId, permissionCode, version, roles);
}

auto UserService::permissions(const ActorContext &context) -> Result<Permissions>
{
    try
    {
        Permissions output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.userRepository();
                if (!repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                auto *roles       = connection.roleRepository();
                auto *permissions = connection.permissionRepository();
                if (!roles || !permissions)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = resolve(context, *repository, false, roles);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                const auto capabilities = authorization_.capabilities(*permissions, actor.value.user.role);
                if (!capabilities.ok)
                    return Result<void>::Failure(capabilities.error, {});
                auto capabilityCodes = std::move(capabilities.value);
                const auto scope     = roles->manageableRoles(actor.value.user.role);
                if (!scope.ok)
                    return Result<void>::Failure(scope.error, {});
                const auto catalog = roles->list();
                if (!catalog.ok)
                    return Result<void>::Failure(catalog.error, {});
                std::vector<std::string> assignable;
                for (const auto &role : catalog.value)
                    if (role.status == "active" &&
                        std::find(scope.value.begin(), scope.value.end(), role.code) != scope.value.end())
                        assignable.push_back(role.code);
                output = {.userId              = actor.value.user.id,
                          .role                = actor.value.user.role,
                          .status              = actor.value.user.status,
                          .forcePasswordChange = actor.value.user.force_password_change && actor.value.user.role == "super_admin",
                          .authzVersion        = actor.value.user.authz_version,
                          .capabilities        = std::move(capabilityCodes),
                          .manageableRoles     = scope.value,
                          .assignableRoles     = std::move(assignable)};
                return Result<void>::Success();
            });
        return operation.ok ? Result<Permissions>::Success(std::move(output)) : Result<Permissions>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<Permissions>(ErrorCode::InternalFailure);
    }
}

auto UserService::list(const ActorContext &context, const ListQuery &query) -> Result<UserPage>
{
    try
    {
        const auto sort = parseSort(query.sort);
        if (!sort || !validSearch(query.search) || (!query.status.empty() && !authorization_.validStatus(query.status)) ||
            query.pageSize == 0)
            return failed<UserPage>(ErrorCode::ValidationError);
        UserPage output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.userRepository();
                if (!repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                auto *roles      = connection.roleRepository();
                const auto actor = resolve(context, *repository, false, roles);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!roles)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto allowed = authorization_.allows(*roles, actor.value.user.role, RBAC::PERMISSIONS::UserRead);
                if (!allowed.ok)
                    return Result<void>::Failure(allowed.error, {});
                if (!allowed.value)
                    return failed<void>(ErrorCode::Forbidden);
                // Restrict the result set to the roles this actor may manage, then intersect
                // with the requested role so administrators cannot enumerate super admins.
                UserFilter filter{.status = query.status, .search = query.search, .sort = *sort, .limit = query.pageSize};
                const auto manageable = authorization_.manageableRoles(*roles, actor.value.user.role);
                if (!manageable.ok)
                    return Result<void>::Failure(manageable.error, {});
                for (const auto &role : manageable.value)
                    if (query.role.empty() || query.role == role)
                        filter.roles.emplace_back(role);
                if (filter.roles.empty())
                    return failed<void>(ErrorCode::Forbidden);
                filter.offset     = static_cast<std::uint64_t>(query.page - 1) * query.pageSize;
                const auto listed = repository->list(filter);
                if (!listed.ok)
                    return Result<void>::Failure(listed.error, {});
                output = listed.value;
                return Result<void>::Success();
            });
        return operation.ok ? Result<UserPage>::Success(std::move(output)) : Result<UserPage>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserPage>(ErrorCode::InternalFailure);
    }
}

auto UserService::get(const ActorContext &context, const std::string &id) -> Result<UserRow>
{
    try
    {
        if (!validUuid(id))
            return failed<UserRow>(ErrorCode::ValidationError);
        UserRow output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.userRepository();
                if (!repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                auto *roles      = connection.roleRepository();
                const auto actor = resolve(context, *repository, false, roles);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!roles)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto allowed = authorization_.allows(*roles, actor.value.user.role, RBAC::PERMISSIONS::UserRead);
                if (!allowed.ok)
                    return Result<void>::Failure(allowed.error, {});
                if (!allowed.value)
                    return failed<void>(ErrorCode::Forbidden);
                const auto found = repository->findById(id);
                if (!found.ok)
                    return Result<void>::Failure(found.error, {});
                if (!found.value)
                    return failed<void>(ErrorCode::ResourceNotFound);
                if (!roles)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto manageable = roles->canManageRole(actor.value.user.role, found.value->role);
                if (!manageable.ok)
                    return Result<void>::Failure(manageable.error, {});
                if (!manageable.value)
                    return failed<void>(ErrorCode::Forbidden);
                output = *found.value;
                return Result<void>::Success();
            });
        return operation.ok ? Result<UserRow>::Success(std::move(output)) : Result<UserRow>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<UserRow>(ErrorCode::InternalFailure);
    }
}

} // namespace SZX::ADMIN::USER
