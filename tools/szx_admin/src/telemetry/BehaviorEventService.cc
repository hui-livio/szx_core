#include "BehaviorEventService.h"

#include "../auth/LoginService.h"
#include "../user/AuditDetails.h"
#include "../user/UserRepository.h"
#include "../user/UserValidation.h"

#include "../rbac/PermissionCodes.h"

#include "JsonHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace SZX::ADMIN::TELEMETRY
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

auto acceptedAt() -> std::string
{
    const auto now = SZX::COMMON::TimeHelper::NowTimestamp();
    std::ostringstream stream;
    stream << SZX::COMMON::TimeHelper::TimestampToString(now) << '.' << std::setw(3) << std::setfill('0') << now % 1000 << "000";
    return stream.str();
}

} // namespace

BehaviorEventService::BehaviorEventService(INFRA::DatabasePool &database, AUTH::LoginService &auth, BehaviorEventWriter &writer) :
    database_(database),
    authorization_(auth),
    writer_(writer)
{
}

auto BehaviorEventService::record(const USER::ActorContext &context, const BehaviorEvent &event) -> Result<std::string>
{
    try
    {
        if (!USER::validContext(context.requestId, context.peerAddress) || !USER::validEventName(event.eventName) ||
            USER::reservedEventName(event.eventName) || (event.result != "success" && event.result != "failure") ||
            event.detailsJson.size() > 4096 || context.userAgent.size() > 512)
            return failed<std::string>(ErrorCode::ValidationError);
        auto details = SZX::COMMON::JsonHelper::ParseRequiredObject(event.detailsJson);
        if (!details)
            return failed<std::string>(ErrorCode::ValidationError);
        USER::redactAuditDetails(*details);

        std::string userId;
        const auto authorized = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users = connection.userRepository();
                if (!users)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = authorization_.resolve(context, *users);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                userId = actor.value.user.id;
                return Result<void>::Success();
            });
        if (!authorized.ok)
            return Result<std::string>::Failure(authorized.error, {});

        const auto id = SZX::COMMON::Uuid::v4().toString();
        const auto queued = writer_.enqueue({.id           = id,
                                             .user_id      = std::move(userId),
                                             .event_name   = event.eventName,
                                             .result       = event.result,
                                             .request_id   = context.requestId,
                                             .ip           = context.peerAddress,
                                             .user_agent   = context.userAgent,
                                             .details_json = details->dump(),
                                             .created_at   = acceptedAt()});
        return queued.ok ? Result<std::string>::Success(id) : Result<std::string>::Failure(queued.error, {});
    } catch (...)
    {
        return failed<std::string>(ErrorCode::InternalFailure);
    }
}

auto BehaviorEventService::list(const USER::ActorContext &context, const BehaviorEventQuery &query) -> Result<BehaviorEventPage>
{
    try
    {
        const bool oneBoundary = query.from.empty() != query.to.empty();
        auto from = query.from.empty() ? std::optional<std::string>{} : USER::databaseTimestamp(query.from);
        auto to   = query.to.empty() ? std::optional<std::string>{} : USER::databaseTimestamp(query.to);
        if (oneBoundary || (query.from.empty() ? false : !from || !to) || query.page == 0 || query.pageSize == 0 ||
            (!query.eventName.empty() && !USER::validEventName(query.eventName)) ||
            (!query.result.empty() && query.result != "success" && query.result != "failure") ||
            (!query.userId.empty() && !USER::validUuid(query.userId)) || !USER::validSearch(query.userSearch) ||
            !USER::validSearch(query.eventSearch))
            return failed<BehaviorEventPage>(ErrorCode::ValidationError);

        if (!from)
        {
            const auto now = SZX::COMMON::TimeHelper::NowTimestamp();
            from = SZX::COMMON::TimeHelper::TimestampToString(now - 24LL * 60 * 60 * 1000);
            to   = SZX::COMMON::TimeHelper::TimestampToString(now);
        }
        else
        {
            const auto fromMs = SZX::COMMON::TimeHelper::StringToTimestamp(*from);
            const auto toMs   = SZX::COMMON::TimeHelper::StringToTimestamp(*to);
            if (!fromMs || !toMs || *toMs < *fromMs || *toMs - *fromMs > 31LL * 24 * 60 * 60 * 1000)
                return failed<BehaviorEventPage>(ErrorCode::ValidationError);
        }

        BehaviorEventPage output;
        const auto operation = database_.withConnection(
            [&](INFRA::DatabaseConnection &connection) -> Result<void>
            {
                auto *users      = connection.userRepository();
                auto *repository = connection.behaviorEventRepository();
                if (!users || !repository)
                    return failed<void>(ErrorCode::DatabaseQueryFailed);
                const auto actor = authorization_.resolve(context, *users);
                if (!actor.ok)
                    return Result<void>::Failure(actor.error, {});
                if (!authorization_.allows(actor.value.user.role, RBAC::PERMISSIONS::BehaviorRead))
                    return failed<void>(ErrorCode::Forbidden);
                const auto queried = repository->query({.from        = *from,
                                                        .to          = *to,
                                                        .eventName   = query.eventName,
                                                        .result      = query.result,
                                                        .userId      = query.userId,
                                                        .limit       = query.pageSize,
                                                        .offset      = static_cast<std::uint64_t>(query.page - 1) * query.pageSize,
                                                        .userSearch  = query.userSearch,
                                                        .eventSearch = query.eventSearch});
                if (!queried.ok)
                    return Result<void>::Failure(queried.error, {});
                output = queried.value;
                return Result<void>::Success();
            });
        return operation.ok ? Result<BehaviorEventPage>::Success(std::move(output)) :
                              Result<BehaviorEventPage>::Failure(operation.error, {});
    } catch (...)
    {
        return failed<BehaviorEventPage>(ErrorCode::InternalFailure);
    }
}
} // namespace SZX::ADMIN::TELEMETRY
