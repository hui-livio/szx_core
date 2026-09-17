#include "Routes.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RestApi.h"

#include <string>

namespace SZX::ADMIN
{
auto registerHealthRoutes(std::string_view apiPrefix,
                          const std::function<SZX::COMMON::Result<void>(std::string, INFRA::HttpHandler)> &registerGet)
    -> SZX::COMMON::Result<void>
{
    try
    {
        const auto path    = apiPrefix == "/" ? std::string{"/health"} : std::string{apiPrefix} + "/health";
        const auto handler = [](const INFRA::HttpRequest &request)
        {
            return INFRA::HttpTransport::healthResponse(request.requestId);
        };
        auto result = registerGet(path, handler);
        return result.ok ? registerGet(path + "/", handler) : result;
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}

auto registerRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database) -> SZX::COMMON::Result<void>
{
    try
    {
        return transport.registerApiRoute(
            "GET", "/ready",
            [&database](const INFRA::HttpRequest &request)
            {
                const auto ping = database.ping();
                if (!ping.ok)
                    return INFRA::RestApi::error(INFRA::ErrorCode::DatabaseUnavailable, request.requestId);
                return INFRA::RestApi::success(SZX::COMMON::Json{{"status", "ready"}}, request.requestId);
            });
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN
