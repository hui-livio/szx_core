#include "RoleService.h"

#include "../auth/LoginService.h"
#include "../infra/DatabaseUnitOfWork.h"
#include "PermissionRepository.h"
#include "../user/UserValidation.h"
#include "PermissionCodes.h"

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

auto validStatus(std::string_view value) noexcept -> bool
{
    return value == "active" || value == "disabled" || value == "archived";
}

auto normalizedIds(const std::vector<std::string> &ids) -> std::optional<std::vector<std::string>>
{
    std::vector<std::string> result = ids;
    for (const auto &id : result)
        if (!USER::validUuid(id))
            return std::nullopt;
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        return std::nullopt;
    return result;
}

auto authorize(AuthorizationService &authorization, const USER::ActorContext &context, INFRA::DatabaseUnitOfWork &unit,
               std::string_view permission) -> Result<AuthorizedActor>
{
    auto *users = unit.users();
    auto *roles = unit.roles();
    if (!users || !roles)
        return failed<AuthorizedActor>(ErrorCode::DatabaseQueryFailed);
    const auto actor = authorization.resolve(context, *users, false, roles);
    if (!actor.ok)
        return Result<AuthorizedActor>::Failure(actor.error, {});
    const auto allowed = authorization.allows(*roles, actor.value.user.role, permission);
    if (!allowed.ok)
        return Result<AuthorizedActor>::Failure(allowed.error, {});
    return allowed.value ? actor : failed<AuthorizedActor>(ErrorCode::Forbidden);
}
} // namespace

RoleService::RoleService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    authorization_(auth)
{
}

auto RoleService::list(const USER::ActorContext &context, bool includeArchived) -> Result<std::vector<RoleRow>>
{
    std::vector<RoleRow> output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository = unit.roles();
        auto *users      = unit.users();
        if (!repository || !users)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleRead);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const auto listed = repository->list(includeArchived);
        if (!listed.ok)
            return Result<void>::Failure(listed.error, {});
        output = listed.value;
        return Result<void>::Success();
    });
    return operation.ok ? Result<std::vector<RoleRow>>::Success(std::move(output)) :
                          Result<std::vector<RoleRow>>::Failure(operation.error, {});
}

auto RoleService::get(const USER::ActorContext &context, std::string_view id) -> Result<RoleRow>
{
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
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
        output = *found.value;
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}

auto RoleService::create(const USER::ActorContext &context, const NewRoleInput &input) -> Result<RoleRow>
{
    if (!USER::validRoleCode(input.code) || !USER::validDisplayName(input.name) || input.description.size() > 512)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository = unit.roles();
        if (!repository)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto begun = unit.begin();
        if (!begun.ok)
            return begun;
        const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleCreate);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const RoleRow row{.id           = SZX::COMMON::Uuid::v4().toString(),
                          .code         = input.code,
                          .name         = input.name,
                          .description  = input.description,
                          .status       = "active",
                          .version      = 1,
                          .authzVersion = 1};
        const auto inserted = repository->insert(NewRole{row.id, row.code, row.name, row.description, row.status, 1, 1, actor.value.user.id});
        if (!inserted.ok)
            return inserted;
        const auto committed = unit.commit();
        if (!committed.ok)
            return committed;
        output = row;
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}

auto RoleService::updateProfile(const USER::ActorContext &context, std::string_view id, std::string_view name,
                                std::string_view description, std::uint32_t version) -> Result<RoleRow>
{
    if (!USER::validUuid(id) || !USER::validDisplayName(name) || description.size() > 512)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository = unit.roles();
        if (!repository)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto begun = unit.begin();
        if (!begun.ok)
            return begun;
        const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleUpdate);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const auto found = repository->findById(id, true);
        if (!found.ok)
            return Result<void>::Failure(found.error, {});
        if (!found.value)
            return failed<void>(ErrorCode::ResourceNotFound);
        if (found.value->version != version)
            return failed<void>(ErrorCode::ConflictVersion);
        auto row = *found.value;
        row.name = std::string{name};
        row.description = std::string{description};
        ++row.version;
        const auto saved = repository->updateProfile(row.id, row.name, row.description, row.version, actor.value.user.id);
        if (!saved.ok)
            return saved;
        const auto committed = unit.commit();
        if (!committed.ok)
            return committed;
        output = std::move(row);
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}

auto RoleService::changeStatus(const USER::ActorContext &context, std::string_view id, std::string_view status,
                               std::uint32_t version) -> Result<RoleRow>
{
    if (!USER::validUuid(id) || !validStatus(status))
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository = unit.roles();
        if (!repository)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto begun = unit.begin();
        if (!begun.ok)
            return begun;
        const auto actor = authorize(authorization_, context, unit,
                                     status == "archived" ? PERMISSIONS::RoleArchive : PERMISSIONS::RoleStatus);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const auto found = repository->findById(id, true);
        if (!found.ok)
            return Result<void>::Failure(found.error, {});
        if (!found.value)
            return failed<void>(ErrorCode::ResourceNotFound);
        if (found.value->version != version)
            return failed<void>(ErrorCode::ConflictVersion);
        if (found.value->systemKey && *found.value->systemKey == "super_admin" && status != "active")
            return failed<void>(ErrorCode::Forbidden);
        auto row = *found.value;
        row.status = std::string{status};
        ++row.version;
        ++row.authzVersion;
        const auto archivedStamp = SZX::COMMON::TimeHelper::TimestampToString(SZX::COMMON::TimeHelper::NowTimestamp());
        const auto archivedAt = status == "archived" ? std::optional<std::string_view>{archivedStamp} : std::nullopt;
        const auto saved = repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id,
                                                     archivedAt);
        if (!saved.ok)
            return saved;
        const auto committed = unit.commit();
        if (!committed.ok)
            return committed;
        output = std::move(row);
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}

auto RoleService::permissionIds(const USER::ActorContext &context, std::string_view id)
    -> Result<std::vector<std::string>>
{
    if (!USER::validUuid(id))
        return failed<std::vector<std::string>>(ErrorCode::ValidationError);
    std::vector<std::string> output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
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

auto RoleService::manageScopeIds(const USER::ActorContext &context, std::string_view id)
    -> Result<std::vector<std::string>>
{
    if (!USER::validUuid(id))
        return failed<std::vector<std::string>>(ErrorCode::ValidationError);
    std::vector<std::string> output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
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
                                       const std::vector<std::string> &permissionIds, std::uint32_t version)
    -> Result<RoleRow>
{
    if (!USER::validUuid(id))
        return failed<RoleRow>(ErrorCode::ValidationError);
    const auto normalized = normalizedIds(permissionIds);
    if (!normalized)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository  = unit.roles();
        auto *permissions = unit.permissions();
        if (!repository || !permissions)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto begun = unit.begin();
        if (!begun.ok)
            return begun;
        const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RolePermissionAssign);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const auto found = repository->findById(id, true);
        if (!found.ok)
            return Result<void>::Failure(found.error, {});
        if (!found.value)
            return failed<void>(ErrorCode::ResourceNotFound);
        if (found.value->version != version)
            return failed<void>(ErrorCode::ConflictVersion);
        const auto current = repository->permissionIds(id);
        if (!current.ok)
            return Result<void>::Failure(current.error, {});
        for (const auto &permissionId : *normalized)
        {
            const auto permission = permissions->findById(permissionId);
            if (!permission.ok)
                return Result<void>::Failure(permission.error, {});
            if (!permission.value || permission.value->status != "active")
                return failed<void>(ErrorCode::ResourceNotFound);
        }
        auto row = *found.value;
        if (current.value != *normalized)
        {
            ++row.version;
            ++row.authzVersion;
            const auto replaced = repository->replacePermissions(id, *normalized, actor.value.user.id);
            if (!replaced.ok)
                return replaced;
            const auto saved = repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id);
            if (!saved.ok)
                return saved;
        }
        const auto committed = unit.commit();
        if (!committed.ok)
            return committed;
        output = std::move(row);
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}

auto RoleService::replaceManageScopeIds(const USER::ActorContext &context, std::string_view id,
                                        const std::vector<std::string> &targetRoleIds, std::uint32_t version)
    -> Result<RoleRow>
{
    if (!USER::validUuid(id))
        return failed<RoleRow>(ErrorCode::ValidationError);
    const auto normalized = normalizedIds(targetRoleIds);
    if (!normalized)
        return failed<RoleRow>(ErrorCode::ValidationError);
    RoleRow output;
    const auto operation = database_.withConnection([&](INFRA::DatabaseConnection &connection) -> Result<void>
    {
        INFRA::DatabaseUnitOfWork unit(connection);
        auto *repository = unit.roles();
        if (!repository)
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        const auto begun = unit.begin();
        if (!begun.ok)
            return begun;
        const auto actor = authorize(authorization_, context, unit, PERMISSIONS::RoleScopeAssign);
        if (!actor.ok)
            return Result<void>::Failure(actor.error, {});
        const auto found = repository->findById(id, true);
        if (!found.ok)
            return Result<void>::Failure(found.error, {});
        if (!found.value)
            return failed<void>(ErrorCode::ResourceNotFound);
        if (found.value->version != version)
            return failed<void>(ErrorCode::ConflictVersion);
        for (const auto &targetId : *normalized)
        {
            const auto target = repository->findById(targetId);
            if (!target.ok)
                return Result<void>::Failure(target.error, {});
            if (!target.value || target.value->status == "archived")
                return failed<void>(ErrorCode::ResourceNotFound);
        }
        const auto current = repository->manageScopeIds(id);
        if (!current.ok)
            return Result<void>::Failure(current.error, {});
        auto row = *found.value;
        if (current.value != *normalized)
        {
            ++row.version;
            ++row.authzVersion;
            const auto replaced = repository->replaceManageScopes(id, *normalized, actor.value.user.id);
            if (!replaced.ok)
                return replaced;
            const auto saved = repository->updateStatus(row.id, row.status, row.version, row.authzVersion, actor.value.user.id);
            if (!saved.ok)
                return saved;
        }
        const auto committed = unit.commit();
        if (!committed.ok)
            return committed;
        output = std::move(row);
        return Result<void>::Success();
    });
    return operation.ok ? Result<RoleRow>::Success(std::move(output)) : Result<RoleRow>::Failure(operation.error, {});
}
} // namespace SZX::ADMIN::RBAC
