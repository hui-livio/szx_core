#include "AuditService.h"

#include "../auth/LoginService.h"
#include "../user/AuditEventLabels.h"
#include "../user/UserRepository.h"
#include "../user/UserValidation.h"

#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Policy/Policy.h"

namespace SZX::ADMIN::AUDIT
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
} // namespace

AuditService::AuditService(INFRA::DatabasePool &database, AUTH::LoginService &auth) :
    database_(database),
    auth_(auth)
{
}

auto AuditService::list(const USER::ActorContext &actor, const AuditQuery &query) -> Result<AuditPage>
{
    return this->query(actor, query, false);
}

auto AuditService::listLegacy(const USER::ActorContext &actor, const AuditQuery &query) -> Result<AuditPage>
{
    return this->query(actor, query, true);
}

auto AuditService::query(const USER::ActorContext &context, const AuditQuery &query, bool legacy) -> Result<AuditPage>
{
    try
    {
        const auto from = query.from.empty() ? std::optional<std::string>{std::string{}} : USER::databaseTimestamp(query.from);
        const auto to   = query.to.empty() ? std::optional<std::string>{std::string{}} : USER::databaseTimestamp(query.to);
        if (!from || !to || query.page == 0 || query.pageSize == 0 ||
            (!query.eventName.empty() && !USER::validEventName(query.eventName)) ||
            (!query.result.empty() && !USER::validAuditResult(query.result)) ||
            (!query.actorUserId.empty() && !USER::validUuid(query.actorUserId)) || !USER::validSearch(query.actorSearch) ||
            !USER::validSearch(query.eventSearch))
            return failed<AuditPage>(ErrorCode::ValidationError);

        AuditPage output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users      = connection.userRepository();
                auto *repository = connection.auditRepository();
                if (!users || !repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto claims = auth_.verifyAccess(context.accessToken);
                if (!claims.ok || !USER::validUuid(claims.value.subject))
                    return failed<void>(claims.ok ? ErrorCode::AuthRequired : static_cast<ErrorCode>(claims.error));
                const auto found = users->findById(claims.value.subject);
                if (!found.ok)
                    return Result<void>::Failure(found.error, {});
                if (!found.value || found.value->status != "active" || found.value->authz_version <= 0 ||
                    static_cast<std::uint64_t>(found.value->authz_version) != claims.value.authzVersion)
                    return failed<void>(ErrorCode::AuthRequired);
                if ((found.value->role == "super_admin" && found.value->force_password_change) ||
                    !POLICY::allows(found.value->role, POLICY::Capability::AuditRead))
                    return failed<void>(ErrorCode::Forbidden);

                AuditFilter filter{.from          = *from,
                                   .to            = *to,
                                   .eventName     = query.eventName,
                                   .result        = query.result,
                                   .actorUserId   = query.actorUserId,
                                   .limit         = query.pageSize,
                                   .offset        = static_cast<std::uint64_t>(query.page - 1) * query.pageSize,
                                   .actorSearch   = query.actorSearch,
                                   .eventSearch   = query.eventSearch};
                if (!query.eventSearch.empty())
                    for (const auto &[code, label] : USER::kAuditEventLabels)
                        if (label.find(query.eventSearch) != std::string_view::npos)
                            filter.eventNames.emplace_back(code);
                const auto queried = legacy ? repository->queryCompatibility(filter) : repository->query(filter);
                if (!queried.ok)
                    return Result<void>::Failure(queried.error, {});
                output = queried.value;
                return Result<void>::Success();
            });
        return operation.ok ? Result<AuditPage>::Success(std::move(output)) : Result<AuditPage>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<AuditPage>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::AUDIT
