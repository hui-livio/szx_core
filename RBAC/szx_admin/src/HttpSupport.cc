#include "HttpSupport.h"
#include "StringHelper.hpp"
#include "SzxAdmin/Infra/RestApi.h"

#include <algorithm>

namespace SZX::ADMIN
{
auto originAllowed(std::string_view allowedOrigin, const INFRA::HttpRequest &request) -> bool
{
    if (allowedOrigin == "*")
        return true;
    const auto supplied = request.header("Origin");
    if (!supplied)
        return true;
    return *supplied == allowedOrigin;
}

auto withCors(INFRA::HttpResponse response, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    response.headers.emplace("Vary", "Origin");
    const auto origin = request.header("Origin");
    if (origin)
    {
        response.headers.emplace("Access-Control-Allow-Origin", std::string{*origin});
        response.headers.emplace("Access-Control-Allow-Credentials", "true");
        response.headers.emplace("Access-Control-Expose-Headers", "X-Request-ID");
    }
    return response;
}

auto preflightHandler(std::string allowedOrigin, std::string methods) -> INFRA::HttpHandler
{
    return [allowedOrigin = std::move(allowedOrigin), methods = std::move(methods)](const INFRA::HttpRequest &request)
    {
        using INFRA::ErrorCode;
        using INFRA::RestApi;
        if (!originAllowed(allowedOrigin, request))
            return RestApi::error(ErrorCode::Forbidden, request.requestId);
        if (const auto method = request.header("Access-Control-Request-Method"))
        {
            const auto supported = SZX::COMMON::StringHelper::Split(methods, ',');
            if (std::none_of(supported.begin(), supported.end(),
                             [&](const auto &candidate) { return SZX::COMMON::StringHelper::Strip(candidate) == *method; }))
                return withCors(RestApi::error(ErrorCode::MethodNotAllowed, request.requestId), request);
        }
        auto response = RestApi::noContent(request.requestId);
        response.headers.emplace("Allow", methods);
        response.headers.emplace("Access-Control-Allow-Methods", methods);
        response.headers.emplace("Access-Control-Allow-Headers",
                                 "Content-Type, Authorization, If-Match, X-Request-ID, X-CSRF-Token");
        return withCors(std::move(response), request);
    };
}

auto allowedQuery(const INFRA::HttpRequest &request, std::span<const std::string_view> names) -> bool
{
    for (const auto &[key, value] : request.query)
    {
        const bool found = std::any_of(names.begin(), names.end(), [&](std::string_view name) { return name == key; });
        if (!found)
            return false;
    }
    return true;
}

auto queryValue(const INFRA::HttpRequest &request, std::string_view name) -> std::string
{
    const auto found = request.query.find(std::string{name});
    return found != request.query.end() ? found->second : std::string{};
}
} // namespace SZX::ADMIN
