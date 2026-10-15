#include "PermissionService.h"

#include "../audit/AuditRepository.h"
#include "../auth/LoginService.h"
#include "../infra/DatabaseUnitOfWork.h"
#include "../user/UserValidation.h"
#include "PermissionCodes.h"

#include "Serializer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

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

struct PermissionAuditDetails final
{
    std::string resource_type{"permission"};
    std::string resource_id;
    std::string code;
};
SZX_REFL(PermissionAuditDetails, resource_type, resource_id, code);
auto permissionAuditDetails(const PermissionRow &row) -> std::string
{
    std::string json;
    SZX::COMMON::Serializer::to_json(PermissionAuditDetails{"permission", row.id, row.code}, json);
    return json;
}

auto validStatus(std::string_view value) noexcept -> bool
{
    return value == "active" || value == "disabled" || value == "archived";
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
} // namespace

PermissionService::PermissionService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    authorization_(auth)
{
}

auto PermissionService::list(const USER::ActorContext &context, bool includeArchived) -> Result<std::vector<PermissionRow>>
try
{
    std::vector<PermissionRow> output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.permissions();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::PermissionRead);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto listed = repository->list(includeArchived);
            if (!listed.ok)
                return Result<void>::Failure(listed.error, {});
            output = listed.value;
            return Result<void>::Success();
        });
    return operation.ok ? Result<std::vector<PermissionRow>>::Success(std::move(output)) :
                          Result<std::vector<PermissionRow>>::Failure(operation.error, {});
} catch (...)
{
    return failed<std::vector<PermissionRow>>(ErrorCode::InternalFailure);
}

auto PermissionService::get(const USER::ActorContext &context, std::string_view id) -> Result<PermissionRow>
try
{
    PermissionRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.permissions();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::PermissionRead);
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
    return operation.ok ? Result<PermissionRow>::Success(std::move(output)) : Result<PermissionRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<PermissionRow>(ErrorCode::InternalFailure);
}

auto PermissionService::create(const USER::ActorContext &context, const NewPermissionInput &input) -> Result<PermissionRow>
try
{
    if (!USER::validEventName(input.code) || !USER::validDisplayName(input.name) || !USER::validEventName(input.module) ||
        input.module.size() > 64 || input.description.size() > 512 || input.code.starts_with("auth.") ||
        input.code.starts_with("user.") || input.code.starts_with("role.") || input.code.starts_with("permission.") ||
        input.code.starts_with("audit.") || input.code.starts_with("behavior.") || input.code.starts_with("system."))
        return failed<PermissionRow>(ErrorCode::ValidationError);
    PermissionRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.permissions();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::PermissionCreate, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto existing = repository->findByCode(input.code);
            if (!existing.ok)
                return Result<void>::Failure(existing.error, {});
            if (existing.value)
                return failed<void>(ErrorCode::PermissionCodeConflict);
            const PermissionRow row{.id          = SZX::COMMON::Uuid::v4().toString(),
                                    .code        = input.code,
                                    .name        = input.name,
                                    .module      = input.module,
                                    .enforcement = "client",
                                    .source      = "custom",
                                    .status      = "active",
                                    .version     = 1,
                                    .description = input.description};
            const auto inserted = repository->insert(NewPermission{row.id, row.code, row.name, input.description, row.module,
                                                                   row.enforcement, row.source, row.status, false});
            if (!inserted.ok)
            {
                const auto duplicate = repository->findByCode(input.code);
                if (duplicate.ok && duplicate.value)
                    return failed<void>(ErrorCode::PermissionCodeConflict);
                return inserted;
            }
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,

                                                .event_name   = "rbac.permission.changed",
                                                .result       = "success",
                                                .request_id   = context.requestId,
                                                .ip           = context.peerAddress,
                                                .details_json = permissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = row;
            return Result<void>::Success();
        });
    return operation.ok ? Result<PermissionRow>::Success(std::move(output)) : Result<PermissionRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<PermissionRow>(ErrorCode::InternalFailure);
}

auto PermissionService::updateProfile(const USER::ActorContext &context, std::string_view id, std::string_view name,
                                      std::string_view description, std::uint32_t version) -> Result<PermissionRow>
try
{
    if (!USER::validUuid(id) || !USER::validDisplayName(name) || description.size() > 512)
        return failed<PermissionRow>(ErrorCode::ValidationError);
    PermissionRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.permissions();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto actor = authorize(authorization_, context, unit, PERMISSIONS::PermissionUpdate, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->version != version || found.value->source == "system" || found.value->status == "archived")
                return found.value->source == "system" || found.value->status == "archived" ?
                    failed<void>(ErrorCode::ProtectedPermission) :
                    failed<void>(ErrorCode::ConflictVersion);
            auto row        = *found.value;
            row.name        = std::string{name};
            row.description = std::string{description};
            ++row.version;
            const auto saved = repository->updateProfile(row.id, row.name, description, row.version, actor.value.user.id);
            if (!saved.ok)
                return saved;
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,

                                                .event_name   = "rbac.permission.changed",
                                                .result       = "success",
                                                .request_id   = context.requestId,
                                                .ip           = context.peerAddress,
                                                .details_json = permissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<PermissionRow>::Success(std::move(output)) : Result<PermissionRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<PermissionRow>(ErrorCode::InternalFailure);
}

auto PermissionService::changeStatus(const USER::ActorContext &context, std::string_view id, std::string_view status,
                                     std::uint32_t version) -> Result<PermissionRow>
try
{
    if (!USER::validUuid(id) || !validStatus(status))
        return failed<PermissionRow>(ErrorCode::ValidationError);
    PermissionRow output;
    const auto operation = database_.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            INFRA::DatabaseUnitOfWork unit(connection);
            auto *repository = unit.permissions();
            auto *audit      = unit.audit();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto begun = unit.begin();
            if (!begun.ok)
                return begun;
            const auto actor =
                authorize(authorization_, context, unit,
                          status == "archived" ? PERMISSIONS::PermissionArchive : PERMISSIONS::PermissionStatus, true);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto found = repository->findById(id, true);
            if (!found.ok)
                return Result<void>::Failure(found.error, {});
            if (!found.value)
                return failed<void>(ErrorCode::ResourceNotFound);
            if (found.value->version != version)
                return failed<void>(ErrorCode::ConflictVersion);
            if (found.value->source == "system" || found.value->status == "archived")
                return failed<void>(ErrorCode::ProtectedPermission);
            if (status == "archived")
            {
                const auto bound = repository->roleBindingCount(id);
                if (!bound.ok)
                    return Result<void>::Failure(bound.error, {});
                if (bound.value)
                    return failed<void>(ErrorCode::PermissionInUse);
            }
            auto row   = *found.value;
            row.status = std::string{status};
            ++row.version;
            const auto archivedStamp = SZX::COMMON::TimeHelper::TimestampToString(SZX::COMMON::TimeHelper::NowTimestamp());
            const auto archivedAt    = status == "archived" ? std::optional<std::string_view>{archivedStamp} : std::nullopt;
            const auto saved         = repository->updateStatus(row.id, row.status, row.version, actor.value.user.id, archivedAt);
            if (!saved.ok)
                return saved;
            const auto bumped = repository->bumpRoleAuthzVersions(row.id);
            if (!bumped.ok)
                return bumped;
            const auto audited = audit->append({.id            = SZX::COMMON::Uuid::v4().toString(),
                                                .actor_user_id = actor.value.user.id,

                                                .event_name   = "rbac.permission.changed",
                                                .result       = "success",
                                                .request_id   = context.requestId,
                                                .ip           = context.peerAddress,
                                                .details_json = permissionAuditDetails(row)});
            if (!audited.ok)
                return audited;
            const auto committed = unit.commit();
            if (!committed.ok)
                return committed;
            output = std::move(row);
            return Result<void>::Success();
        });
    return operation.ok ? Result<PermissionRow>::Success(std::move(output)) : Result<PermissionRow>::Failure(operation.error, {});
} catch (...)
{
    return failed<PermissionRow>(ErrorCode::InternalFailure);
}
} // namespace SZX::ADMIN::RBAC
