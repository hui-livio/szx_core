#include "RoleService.h"

#include "../audit/AuditRepository.h"
#include "../auth/LoginService.h"
#include "../infra/DatabaseUnitOfWork.h"
#include "../user/UserValidation.h"
#include "PermissionCodes.h"
#include "PermissionRepository.h"

#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <algorithm>

namespace SZX::ADMIN::RBAC
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

struct RolePermissionAuditDetails final
{
    std::string resource_type{"role"};
    std::string resource_id;
    std::uint32_t authz_version{};
};
SZX_REFL(RolePermissionAuditDetails, resource_type, resource_id, authz_version);
auto rolePermissionAuditDetails(const RoleRow &row) -> std::string
{
    std::string json;
    SZX::COMMON::Serializer::to_json(RolePermissionAuditDetails{"role", row.id, row.authzVersion}, json);
    return json;
}

auto validStatus(std::string_view value) noexcept -> bool
{
    return value == "active" || value == "disabled" || value == "archived";
}

auto normalizedIds(const std::vector<std::string> &ids) -> std::optional<std::vector<std::string>>
{
    std::vector<std::string> result = ids;
    for (auto &id : result)
    {
        if (!USER::validUuid(id))
            return std::nullopt;
        const auto parsed = SZX::COMMON::Uuid::parse(id);
        if (!parsed)
            return std::nullopt;
        id = parsed->toString();
    }
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        return std::nullopt;
    return result;
}

auto authorize(AuthorizationService &authorization, const USER::ActorContext &context, INFRA::DatabaseUnitOfWork &unit,
               std::string_view permission, bool lockActor = false) -> Result<AuthorizedActor>
{
    auto *users = unit.users();
    auto *roles = unit.roles();
    if (!users || !roles)
        return failed<AuthorizedActor>(ErrorCode::DatabaseQueryFailed);
    const auto actor = authorization.resolve(context, *users, lockActor, roles);
    if (!actor.ok)
        return Result<AuthorizedActor>::Failure(actor.error, {});
    const auto allowed = authorization.allows(*roles, actor.value.user.role, permission);
    if (!allowed.ok)
        return Result<AuthorizedActor>::Failure(allowed.error, {});
    return allowed.value ? actor : failed<AuthorizedActor>(ErrorCode::Forbidden);
}
auto lockLifecycle(INFRA::DatabaseConnection &connection) -> Result<void>
{
    const auto root = connection.bootstrapCompleted();
    if (!root.ok)
        return Result<void>::Failure(root.error, {});
    if (!root.value)
        return failed<void>(ErrorCode::DatabaseQueryFailed);
    return Result<void>::Success();
}
auto lockRoleSet(RoleRepository &repository, std::vector<std::string> ids) -> Result<void>
{
    for (auto &id : ids)
    {
        const auto uuid = SZX::COMMON::Uuid::parse(id);
        if (!uuid)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        id = uuid->toString();
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (const auto &id : ids)
    {
        const auto role = repository.findById(id, true);
        if (!role.ok)
            return Result<void>::Failure(role.error, {});
        if (!role.value)
            return failed<void>(ErrorCode::ResourceNotFound);
    }
    return Result<void>::Success();
}
auto decorateRole(RoleRepository &repository, RoleRow &row, std::string_view actorCode) -> Result<void>
{
    const auto counts = repository.usage(row.id);
    const auto scope  = repository.canManageRole(actorCode, row.code);
    if (!counts.ok || !scope.ok)
        return Result<void>::Failure(!counts.ok ? counts.error : scope.error, {});
    row.manageable = scope.value;
    if (!counts.value.empty())
    {
        row.userCount       = counts.value.front().userCount;
        row.permissionCount = counts.value.front().permissionCount;
    }
    return Result<void>::Success();
}
} // namespace

RoleService::RoleService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    authorization_(auth)
{
}

auto RoleService::list(const USER::ActorContext &context, bool includeArchived) -> Result<std::vector<RoleRow>>
try
{
    std::vector<RoleRow> output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            auto *users      = unit.users();
            if (!repository || !users || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleRead);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto listed = repository->list(includeArchived);
            if (!listed.ok)
                return Result<void>::Failure(listed.error, {});
            output            = listed.value;
            const auto counts = repository->usage();
            const auto scope  = repository->manageableRoles(actor.value.user.role);
            if (!counts.ok || !scope.ok)
                return Result<void>::Failure(!counts.ok ? counts.error : scope.error, {});
            for (auto &row : output)
            {
                row.manageable = std::find(scope.value.begin(), scope.value.end(), row.code) != scope.value.end();
                for (const auto &count : counts.value)
                    if (count.id == row.id)
                    {
                        row.userCount       = count.userCount;
                        row.permissionCount = count.permissionCount;
                        break;
                    }
            }
            return Result<void>::Success();
        });
    return operation.ok ? Result<std::vector<RoleRow>>::Success(std::move(output)) :
                          Result<std::vector<RoleRow>>::Failure(operation.error, {});
} catch (...)
{
    return failed<std::vector<RoleRow>>(ErrorCode::InternalFailure);
}

auto RoleService::get(const USER::ActorContext &context, std::string_view id) -> Result<RoleRow>
try
{
    if (!USER::validUuid(id))
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleRead);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            output            = *found.value;
            const auto counts = repository->usage(id);
            const auto scope  = repository->canManageRole(actor.value.user.role, output.code);
            if (!counts.ok || !scope.ok)
                return Result<void>::Failure(!counts.ok ? counts.error : scope.error, {});
            output.manageable = scope.value;
            if (!counts.value.empty())
            {
                output.userCount       = counts.value.front().userCount;
                output.permissionCount = counts.value.front().permissionCount;
            }
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}

auto RoleService::create(const USER::ActorContext &context, const NewRoleInput &input) -> Result<RoleRow>
try
{
    if (!USER::validRoleCode(input.code) || !USER::validDisplayName(input.name) || USER::characterCount(input.description) > 512)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto lockedLifecycle = lockLifecycle(connection);
            if (!lockedLifecycle.ok)
                return lockedLifecycle;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleCreate, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto existing = repository->findByCode(input.code);
            if (!existing.ok)
                return Result<void>::Failure(existing.error, {});
            if (existing.value)
                return failed<void>(ErrorCode::RoleCodeConflict);
            RoleRow row{.id           = SZX::COMMON::Uuid::v4().toString(),
                        .code         = input.code,
                        .name         = input.name,
                        .description  = input.description,
                        .status       = "active",
                        .version      = 1,
                        .authzVersion = 1};
            const auto inserted =
                repository->insert(NewRole{row.id, row.code, row.name, row.description, row.status, 1, 1, actor.value.user.id});
            if (!inserted.ok)
            {
                const auto duplicate = repository->findByCode(input.code);
                if (duplicate.ok && duplicate.value)
                    return failed<void>(ErrorCode::RoleCodeConflict);
                return inserted;
            }
            const auto all = repository->list();
            if (!all.ok)
                return Result<void>::Failure(all.error, {});
            for (const auto &manager : all.value)
                if (manager.systemKey && *manager.systemKey == "super_admin")
                {
                    const auto scopes = repository->manageScopeIds(manager.id);
                    if (!scopes.ok)
                        return Result<void>::Failure(scopes.error, {});
                    auto plan = scopes.value;
                    plan.push_back(manager.id);
                    const auto lockedPlan = lockRoleSet(*repository, std::move(plan));
                    if (!lockedPlan.ok)
                        return lockedPlan;
                    const auto locked = repository->findById(manager.id, true);
                    if (!locked.ok || !locked.value)
                        return failed<void>(ErrorCode::DatabaseQueryFailed);
                    auto targets = scopes.value;
                    targets.push_back(row.id);
                    const auto savedScopes = repository->replaceManageScopes(manager.id, targets, actor.value.user.id);
                    if (!savedScopes.ok)
                        return savedScopes;
                    const auto savedManager =
                        repository->updateStatus(manager.id, locked.value->status, locked.value->version + 1,
                                                 locked.value->authzVersion + 1, actor.value.user.id);
                    if (!savedManager.ok)
                        return savedManager;
                }
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,
                                                .event_name    = "rbac.role.changed",
                                                .result        = "success",
                                                .request_id    = context.requestId,
                                                .ip            = context.peerAddress,
                                                .details_json  = rolePermissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto decorated = decorateRole(*repository, row, actor.value.user.role);
            if (!decorated.ok)
                return decorated;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = row;
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}

auto RoleService::updateProfile(const USER::ActorContext &context, std::string_view id, std::string_view name,
                                std::string_view description, std::uint32_t version) -> Result<RoleRow>
try
{
    if (!USER::validUuid(id) || !USER::validDisplayName(name) || USER::characterCount(description) > 512)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleUpdate, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->status == "archived")
                return failed<void>(ErrorCode::ProtectedRole);
            const auto inScope = repository->canManageRole(actor.value.user.role, found.value->code);
            if (!inScope.ok)
                return Result<void>::Failure(inScope.error, {});
            if (!inScope.value)
                return failed<void>(ErrorCode::Forbidden);
            if (found.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            auto row        = *found.value;
            row.name        = std::string{name};
            row.description = std::string{description};
            ++row.version;
            const auto saved = repository->updateProfile(row.id, row.name, row.description, row.version, actor.value.user.id);
            if (!saved.ok)
                return saved;
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,
                                                .event_name    = "rbac.role.changed",
                                                .result        = "success",
                                                .request_id    = context.requestId,
                                                .ip            = context.peerAddress,
                                                .details_json  = rolePermissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto decorated = decorateRole(*repository, row, actor.value.user.role);
            if (!decorated.ok)
                return decorated;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}

auto RoleService::changeStatus(const USER::ActorContext &context, std::string_view id, std::string_view status,
                               std::uint32_t version) -> Result<RoleRow>
try
{
    if (!USER::validUuid(id) || !validStatus(status))
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto lockedLifecycle = lockLifecycle(connection);
            if (!lockedLifecycle.ok)
                return lockedLifecycle;
            const auto actor = authorize(authorization_, context, unit,
                                         status == "archived" ? PERMISSIONS::RoleArchive : PERMISSIONS::RoleStatus, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            if (status == "archived")
            {
                const auto managers = repository->managerIds(id);
                const auto targets  = repository->manageScopeIds(id);
                if (!managers.ok || !targets.ok)
                    return Result<void>::Failure(!managers.ok ? managers.error : targets.error, {});
                auto plan = managers.value;
                plan.insert(plan.end(), targets.value.begin(), targets.value.end());
                plan.push_back(std::string{id});
                const auto lockedPlan = lockRoleSet(*repository, std::move(plan));
                if (!lockedPlan.ok)
                    return lockedPlan;
            }
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->status == "archived" || (status == "archived" && found.value->systemKey))
                return failed<void>(ErrorCode::ProtectedRole);
            const auto inScope = repository->canManageRole(actor.value.user.role, found.value->code);
            if (!inScope.ok)
                return Result<void>::Failure(inScope.error, {});
            if (!inScope.value)
                return failed<void>(ErrorCode::Forbidden);
            if (found.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            if (found.value->systemKey && *found.value->systemKey == "super_admin" && status != "active")
                return failed<void>(ErrorCode::ProtectedRole);
            if (status == "archived")
            {
                const auto counts = repository->usage(id);
                if (!counts.ok)
                    return Result<void>::Failure(counts.error, {});
                if (!counts.value.empty() && counts.value.front().userCount)
                    return failed<void>(ErrorCode::RoleInUse);
            }
            auto row   = *found.value;
            row.status = std::string{status};
            ++row.version;
            ++row.authzVersion;
            const auto archivedStamp = SZX::COMMON::TimeHelper::TimestampToString(SZX::COMMON::TimeHelper::NowTimestamp());
            const auto archivedAt    = status == "archived" ? std::optional<std::string_view>{archivedStamp} : std::nullopt;
            const auto saved =
                repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id, archivedAt);
            if (!saved.ok)
                return saved;
            if (status == "archived")
            {
                const auto bumped = repository->bumpManagerAuthzVersions(row.id);
                if (!bumped.ok)
                    return bumped;
                const auto removed = repository->removeTargetScopes(row.id);
                if (!removed.ok)
                    return removed;
                const auto clearedPermissions = repository->replacePermissions(row.id, {}, actor.value.user.id);
                if (!clearedPermissions.ok)
                    return clearedPermissions;
                const auto clearedScopes = repository->replaceManageScopes(row.id, {}, actor.value.user.id);
                if (!clearedScopes.ok)
                    return clearedScopes;
            }
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,
                                                .event_name    = "rbac.role.changed",
                                                .result        = "success",
                                                .request_id    = context.requestId,
                                                .ip            = context.peerAddress,
                                                .details_json  = rolePermissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto decorated = decorateRole(*repository, row, actor.value.user.role);
            if (!decorated.ok)
                return decorated;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}

auto RoleService::permissionIds(const USER::ActorContext &context, std::string_view id) -> Result<std::vector<std::string>>
{
    if (!USER::validUuid(id))
        return failed<std::vector<std::string>>(ErrorCode::ValidationError);
    std::vector<std::string> output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            if (!repository)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleRead);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            const auto ids = repository->permissionIds(id);
            if (!ids.ok)
                return Result<void>::Failure(ids.error, {});
            output = ids.value;
            return Result<void>::Success();
        });
    return operation.ok ? Result<std::vector<std::string>>::Success(std::move(output)) :
                          Result<std::vector<std::string>>::Failure(operation.error, {});
}

auto RoleService::manageScopeIds(const USER::ActorContext &context, std::string_view id) -> Result<std::vector<std::string>>
{
    if (!USER::validUuid(id))
        return failed<std::vector<std::string>>(ErrorCode::ValidationError);
    std::vector<std::string> output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            if (!repository)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleRead);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            const auto ids = repository->manageScopeIds(id);
            if (!ids.ok)
                return Result<void>::Failure(ids.error, {});
            output = ids.value;
            return Result<void>::Success();
        });
    return operation.ok ? Result<std::vector<std::string>>::Success(std::move(output)) :
                          Result<std::vector<std::string>>::Failure(operation.error, {});
}

auto RoleService::replacePermissionIds(const USER::ActorContext &context, std::string_view id,
                                       const std::vector<std::string> &permissionIds, std::uint32_t version) -> Result<RoleRow>
try
{
    if (!USER::validUuid(id))
        return failed<RoleRow>(ErrorCode::ValidationError);
    const auto normalized = normalizedIds(permissionIds);
    if (!normalized)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository  = unit.roles();
            auto *audit       = unit.audit();
            auto *permissions = unit.permissions();
            if (!repository || !permissions || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RolePermissionAssign, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            // Global order for this operation: permission UUIDs, then the role row.
            // Include removed IDs as well: DELETE of a role binding must not race
            // the permission status transaction that updates the same role.
            const auto snapshotRole = repository->findById(id);
            if (!snapshotRole.ok)
                return Result<void>::Failure(snapshotRole.error, {});
            if (!snapshotRole.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (snapshotRole.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            const auto snapshotIds = repository->permissionIds(id);
            if (!snapshotIds.ok)
                return Result<void>::Failure(snapshotIds.error, {});
            const auto beforeIds = normalizedIds(snapshotIds.value);
            if (!beforeIds)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            auto lockIds = *beforeIds;
            lockIds.insert(lockIds.end(), normalized->begin(), normalized->end());
            std::sort(lockIds.begin(), lockIds.end());
            lockIds.erase(std::unique(lockIds.begin(), lockIds.end()), lockIds.end());
            std::vector<PermissionRow> lockedPermissions;
            lockedPermissions.reserve(lockIds.size());
            for (const auto &permissionId : lockIds)
            {
                const auto permission = permissions->findById(permissionId, true);
                if (!permission.ok)
                    return Result<void>::Failure(permission.error, {});
                if (!permission.value)
                    return failed<void>(ErrorCode::ResourceNotFound);
                lockedPermissions.push_back(*permission.value);
            }
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->status == "archived")
                return failed<void>(ErrorCode::ProtectedRole);
            const auto inScope = repository->canManageRole(actor.value.user.role, found.value->code);
            if (!inScope.ok)
                return Result<void>::Failure(inScope.error, {});
            if (!inScope.value)
                return failed<void>(ErrorCode::Forbidden);
            if (found.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            const auto current = repository->permissionIds(id);
            if (!current.ok)
                return Result<void>::Failure(current.error, {});
            // Do not apply a stale lock plan if another writer changed bindings.
            const auto currentIds = normalizedIds(current.value);
            if (!currentIds)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            if (*currentIds != *beforeIds)
                return failed<void>(ErrorCode::ConflictVersion);
            for (const auto &permission : lockedPermissions)
            {
                if (!std::binary_search(normalized->begin(), normalized->end(), permission.id))
                    continue;
                if (permission.status == "archived" ||
                    (permission.status != "active" &&
                     std::find(currentIds->begin(), currentIds->end(), permission.id) == currentIds->end()))
                    return failed<void>(ErrorCode::ResourceNotFound);
            }
            if (found.value->systemKey && *found.value->systemKey == "super_admin")
            {
                const auto catalog = permissions->list();
                if (!catalog.ok)
                    return Result<void>::Failure(catalog.error, {});
                for (const auto &permission : catalog.value)
                    if (permission.requiredForSuperAdmin &&
                        !std::binary_search(normalized->begin(), normalized->end(), permission.id))
                        return failed<void>(ErrorCode::SuperAdminSecurityFloor);
            }
            auto row = *found.value;
            if (*currentIds != *normalized)
            {
                ++row.version;
                ++row.authzVersion;
                const auto replaced = repository->replacePermissions(id, *normalized, actor.value.user.id);
                if (!replaced.ok)
                    return replaced;
                const auto saved =
                    repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id);
                if (!saved.ok)
                    return saved;
            }
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,
                                                .event_name    = "rbac.role.changed",
                                                .result        = "success",
                                                .request_id    = context.requestId,
                                                .ip            = context.peerAddress,
                                                .details_json  = rolePermissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto decorated = decorateRole(*repository, row, actor.value.user.role);
            if (!decorated.ok)
                return decorated;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}

auto RoleService::replaceManageScopeIds(const USER::ActorContext &context, std::string_view id,
                                        const std::vector<std::string> &targetRoleIds, std::uint32_t version) -> Result<RoleRow>
try
{
    if (!USER::validUuid(id))
        return failed<RoleRow>(ErrorCode::ValidationError);
    const auto normalized = normalizedIds(targetRoleIds);
    if (!normalized)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.roles();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto lockedLifecycle = lockLifecycle(connection);
            if (!lockedLifecycle.ok)
                return lockedLifecycle;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleScopeAssign, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto current = repository->manageScopeIds(id);
            if (!current.ok)
                return Result<void>::Failure(current.error, {});
            auto plan = current.value;
            plan.insert(plan.end(), normalized->begin(), normalized->end());
            plan.push_back(std::string{id});
            const auto lockedPlan = lockRoleSet(*repository, std::move(plan));
            if (!lockedPlan.ok)
                return lockedPlan;
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->status == "archived")
                return failed<void>(ErrorCode::ProtectedRole);
            const auto inScope = repository->canManageRole(actor.value.user.role, found.value->code);
            if (!inScope.ok)
                return Result<void>::Failure(inScope.error, {});
            if (!inScope.value)
                return failed<void>(ErrorCode::Forbidden);
            if (found.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            if (found.value->systemKey && *found.value->systemKey == "super_admin")
            {
                const auto all = repository->list();
                if (!all.ok)
                    return Result<void>::Failure(all.error, {});
                for (const auto &target : all.value)
                    if (!std::binary_search(normalized->begin(), normalized->end(), target.id))
                        return failed<void>(ErrorCode::SuperAdminSecurityFloor);
            }
            for (const auto &targetId : *normalized)
            {
                const auto target = repository->findById(targetId);
                if (!target.ok)
                    return Result<void>::Failure(target.error, {});
                if (!target.value || target.value->status == "archived")
                    return failed<void>(ErrorCode::ResourceNotFound);
                if (target.value->status != "active" && found.value->systemKey != "super_admin" &&
                    std::find(current.value.begin(), current.value.end(), targetId) == current.value.end())
                    return failed<void>(ErrorCode::ResourceNotFound);
            }
            auto row = *found.value;
            if (current.value != *normalized)
            {
                ++row.version;
                ++row.authzVersion;
                const auto replaced = repository->replaceManageScopes(id, *normalized, actor.value.user.id);
                if (!replaced.ok)
                    return replaced;
                const auto saved =
                    repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id);
                if (!saved.ok)
                    return saved;
            }
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,
                                                .event_name    = "rbac.role.changed",
                                                .result        = "success",
                                                .request_id    = context.requestId,
                                                .ip            = context.peerAddress,
                                                .details_json  = rolePermissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto decorated = decorateRole(*repository, row, actor.value.user.role);
            if (!decorated.ok)
                return decorated;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<RoleRow>(ErrorCode::InternalFailure);
}
} // namespace SZX::ADMIN::RBAC
