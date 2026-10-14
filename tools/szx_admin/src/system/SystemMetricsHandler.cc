#include "../HttpSupport.h"
#include "../auth/LoginService.h"
#include "../rbac/AuthorizationService.h"
#include "../rbac/PermissionCodes.h"
#include "SystemMetrics.h"
#include "SystemMetricsHandler.h"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/RestApi.h"

#include <array>
#include <charconv>

namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
namespace
{
using INFRA::ErrorCode;
using INFRA::RestApi;
using SZX::COMMON::Result;
auto authorize(INFRA::DatabasePool &database, AUTH::LoginService &login, const INFRA::HttpRequest &request) -> Result<void>
{
    const auto token = RestApi::bearerToken(request);
    if (!token.ok)
        return Result<void>::Failure(token.error, {});
    USER::ActorContext context{token.value, request.requestId, request.peerAddress,
                               std::string(request.header("User-Agent").value_or(""))};
    RBAC::AuthorizationService authorization(login);
    return database.withConnection(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *users = connection.userRepository();
            auto *roles = connection.roleRepository();
            if (!users || !roles)
                return Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseUnavailable), {});
            const auto actor = authorization.resolve(context, *users, false, roles);
            if (!actor.ok)
                return Result<void>::Failure(actor.error, {});
            const auto allowed = authorization.allows(*roles, actor.value.user.role, RBAC::PERMISSIONS::SystemMetricsRead);
            if (!allowed.ok)
                return Result<void>::Failure(allowed.error, {});
            return allowed.value ? Result<void>::Success() : Result<void>::Failure(static_cast<int>(ErrorCode::Forbidden), {});
        });
}
auto respond(INFRA::DatabasePool &database, AUTH::LoginService &login, SystemMetricsService &metrics,
             const INFRA::HttpRequest &request, bool history) -> INFRA::HttpResponse
{
    try
    {
        const auto auth = authorize(database, login, request);
        if (!auth.ok)
            return RestApi::error(static_cast<ErrorCode>(auth.error), request.requestId);
        constexpr std::array<std::string_view, 1> names{"window_seconds"};
        if (!allowedQuery(request, history ? std::span<const std::string_view>(names) : std::span<const std::string_view>{}))
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        unsigned window = 300;
        if (request.query.contains("window_seconds"))
        {
            const auto value        = queryValue(request, "window_seconds");
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), window);
            if (error != std::errc{} || end != value.data() + value.size() || !window || window > 900)
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
        }
        const auto result = history ? metrics.history(window) : metrics.snapshot();
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        auto response                     = RestApi::success(result.value, request.requestId);
        response.headers["Cache-Control"] = "no-store";
        return response;
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}
} // namespace
auto registerSystemMetricsRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database, AUTH::LoginService &login,
                                 SystemMetricsService &metrics) -> Result<void>
{
    try
    {
        const auto origin = login.config().allowedOrigin;
        for (const bool history : {false, true})
        {
            const std::string path = history ? "/admin/system/metrics/history" : "/admin/system/metrics";
            auto result =
                transport.registerApiRoute("GET", path,
                                           [&, origin, history](const INFRA::HttpRequest &request)
                                           {
                                               if (!originAllowed(origin, request))
                                                   return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                               return withCors(respond(database, login, metrics, request, history), request);
                                           });
            if (!result.ok)
                return result;
            result = transport.registerApiRoute("OPTIONS", path, preflightHandler(origin, "GET"));
            if (!result.ok)
                return result;
        }
        return Result<void>::Success();
    } catch (...)
    {
        return Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
    }
}
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
