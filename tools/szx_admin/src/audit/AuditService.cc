#include "AuditService.h"

#include "../auth/LoginService.h"
#include "../user/AuditEventLabels.h"
#include "../user/UserRepository.h"
#include "../user/UserValidation.h"

#include "../rbac/PermissionCodes.h"
#include "../rbac/RoleRepository.h"

#include "SzxAdmin/Infra/ErrorCode.h"
#include "Logger.h"

#include <chrono>

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
    authorization_(auth)
{
}

auto AuditService::list(const USER::ActorContext &actor, const AuditQuery &query) -> Result<AuditPage>
{
    return this->query(actor, query);
}

auto AuditService::query(const USER::ActorContext &context, const AuditQuery &query) -> Result<AuditPage>
{
    try
    {
        const auto started = std::chrono::steady_clock::now();
        LOGM_DEBUG("szx_admin.audit", "event=query_begin request_id={} page={} page_size={} actor_search={} event_search={}",
                   context.requestId, query.page, query.pageSize, !query.actorSearch.empty(), !query.eventSearch.empty());
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
                auto *roles      = connection.roleRepository();
                if (!roles)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = authorization_.resolve(context, *users, false, roles);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                const auto allowed = authorization_.allows(*roles, actor.value.user.role, RBAC::PERMISSIONS::AuditRead);
                if (!allowed.ok)
                    return Result<void>::Failure(allowed.error, {});
                if (!allowed.value)
                    return failed<void>(ErrorCode::Forbidden);

                AuditFilter filter{.from        = *from,
                                   .to          = *to,
                                   .eventName   = query.eventName,
                                   .result      = query.result,
                                   .actorUserId = query.actorUserId,
                                   .limit       = query.pageSize,
                                   .offset      = static_cast<std::uint64_t>(query.page - 1) * query.pageSize,
                                   .actorSearch = query.actorSearch,
                                   .eventSearch = query.eventSearch};
                if (!query.eventSearch.empty())
                    for (const auto &[code, label] : USER::kAuditEventLabels)
                        if (label.find(query.eventSearch) != std::string_view::npos)
                            filter.eventNames.emplace_back(code);
                const auto queried = repository->query(filter);
                if (!queried.ok)
                    return Result<void>::Failure(queried.error, {});
                output = queried.value;
                return Result<void>::Success();
            });
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
        LOGM_DEBUG("szx_admin.audit", "event=query_complete request_id={} duration_microseconds={} ok={} total={} items={} error={}",
                   context.requestId, elapsed.count(), operation.ok, output.total, output.items.size(), operation.error);
        return operation.ok ? Result<AuditPage>::Success(std::move(output)) : Result<AuditPage>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<AuditPage>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::AUDIT
