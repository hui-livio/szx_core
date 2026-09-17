#include "HttpSupport.h"
#include "StringHelper.hpp"
#include "SzxAdmin/Infra/RestApi.h"
#include "TimeHelper.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

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

auto utcTimestampJson(const std::optional<std::string> &value) -> SZX::COMMON::Json
{
    if (!value)
        return nullptr;
    if (const auto iso = SZX::COMMON::TimeHelper::Iso8601ToTimestamp(*value))
        return *iso;

    const auto &text = *value;
    if (text.size() != 19 && (text.size() < 21 || text.size() > 26 || text[19] != '.'))
        throw std::invalid_argument("Invalid persisted UTC timestamp.");
    const auto seconds = SZX::COMMON::TimeHelper::StringToTimestamp(text.substr(0, 19), "%Y-%m-%d %H:%M:%S", true);
    if (!seconds)
        throw std::invalid_argument("Invalid persisted UTC timestamp.");
    std::int64_t milliseconds{};
    int scale = 100;
    for (std::size_t index = 20; index < text.size(); ++index)
    {
        if (text[index] < '0' || text[index] > '9')
            throw std::invalid_argument("Invalid persisted UTC timestamp.");
        milliseconds += (text[index] - '0') * scale;
        scale        /= 10;
    }
    return *seconds + milliseconds;
}
} // namespace SZX::ADMIN
