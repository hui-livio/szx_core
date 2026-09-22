#include "UserService.h"
#include "../auth/LoginService.h"
#include "UserValidation.h"

#include "SzxAdmin/Infra/ErrorCode.h"

#include <array>
#include <utility>

namespace SZX::ADMIN::USER
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

constexpr std::array<std::string_view, 4> kAllRoles{"operator", "engineer", "admin", "super_admin"};
} // namespace

UserService::UserService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    auth_(auth)
{
}

auto UserService::resolve(const ActorContext &context, UserRepository &repository, bool lock) -> Result<UserRow>
{
    const auto claims = auth_.verifyAccess(context.accessToken);
    if (!claims.ok)
        return Result<UserRow>::Failure(claims.error, {});
    if (!validUuid(claims.value.subject))
        return failed<UserRow>(ErrorCode::AuthRequired);
    const auto found = repository.findById(claims.value.subject, lock);
    if (!found.ok)
        return Result<UserRow>::Failure(found.error, {});
    // Authorization always follows the stored record, never the role claim inside the token.
    if (!found.value || found.value->status != "active" || found.value->authz_version <= 0 ||
        static_cast<std::uint64_t>(found.value->authz_version) != claims.value.authzVersion)
        return failed<UserRow>(ErrorCode::AuthRequired);
    // A frontend redirect is not authorization: pending super admins may only use auth endpoints.
    if (found.value->role == "super_admin" && found.value->force_password_change)
        return failed<UserRow>(ErrorCode::Forbidden);
    return Result<UserRow>::Success(*found.value);
}

auto UserService::authorize(const ActorContext &context, UserRepository &repository, const std::string &targetId,
                            POLICY::Capability required, std::uint32_t version) -> Result<Mutation>
{
    const auto claims = auth_.verifyAccess(context.accessToken);
    if (!claims.ok)
        return Result<Mutation>::Failure(claims.error, {});
    if (!validUuid(claims.value.subject))
        return failed<Mutation>(ErrorCode::AuthRequired);
    const auto locked = repository.lockForMutation(claims.value.subject, targetId);
    if (!locked.ok)
        return Result<Mutation>::Failure(locked.error, {});
    const auto &actor = locked.value.actor;
    if (!actor || actor->status != "active" || actor->authz_version <= 0 ||
        static_cast<std::uint64_t>(actor->authz_version) != claims.value.authzVersion)
        return failed<Mutation>(ErrorCode::AuthRequired);
    if (actor->role == "super_admin" && actor->force_password_change)
        return failed<Mutation>(ErrorCode::Forbidden);
    if (!POLICY::allows(actor->role, required))
        return failed<Mutation>(ErrorCode::Forbidden);
    if (!locked.value.target)
        return failed<Mutation>(ErrorCode::ResourceNotFound);
    if (!POLICY::mayTarget(actor->role, locked.value.target->role))
        return failed<Mutation>(ErrorCode::Forbidden);
    if (locked.value.target->version != static_cast<int>(version))
        return failed<Mutation>(ErrorCode::ConflictVersion);
    return Result<Mutation>::Success({*actor, *locked.value.target, locked.value.activeSuperAdmins});
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
                const auto actor = resolve(context, *repository);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                output = {.userId              = actor.value.id,
                          .role                = actor.value.role,
                          .status              = actor.value.status,
                          .forcePasswordChange = actor.value.force_password_change && actor.value.role == "super_admin",
                          .authzVersion        = actor.value.authz_version,
                          .capabilities        = POLICY::capabilities(actor.value.role)};
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
        if (!sort || !validSearch(query.search) || (!query.status.empty() && !POLICY::validStatus(query.status)) ||
            (!query.role.empty() && !POLICY::validRole(query.role)) || query.pageSize == 0)
            return failed<UserPage>(ErrorCode::ValidationError);
        UserPage output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *repository = connection.userRepository();
                if (!repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = resolve(context, *repository);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!POLICY::allows(actor.value.role, POLICY::Capability::UserRead))
                    return failed<void>(ErrorCode::Forbidden);
                // Restrict the result set to the roles this actor may manage, then intersect
                // with the requested role so administrators cannot enumerate super admins.
                UserFilter filter{.status = query.status, .search = query.search, .sort = *sort, .limit = query.pageSize};
                for (const auto role : kAllRoles)
                    if (POLICY::mayTarget(actor.value.role, role) && (query.role.empty() || query.role == role))
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
                const auto actor = resolve(context, *repository);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!POLICY::allows(actor.value.role, POLICY::Capability::UserRead))
                    return failed<void>(ErrorCode::Forbidden);
                const auto found = repository->findById(id);
                if (!found.ok)
                    return Result<void>::Failure(found.error, {});
                if (!found.value)
                    return failed<void>(ErrorCode::ResourceNotFound);
                if (!POLICY::mayTarget(actor.value.role, found.value->role))
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
