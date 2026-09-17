#include "SzxAdmin/Infra/RestApi.h"

#include "JsonHelper.hpp"
#include "StringHelper.hpp"
#include "SzxAdmin/Infra/RequestContext.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

namespace SZX::ADMIN::INFRA
{
namespace
{
using SZX::COMMON::Json;
using SZX::COMMON::Result;

template <typename T>
auto failure(ErrorCode code = ErrorCode::ValidationError) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto positiveInteger(std::string_view text, std::uint32_t maximum) -> std::optional<std::uint32_t>
{
    if (text.empty() || text.size() > 10 || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
        return std::nullopt;
    const auto parsed = SZX::COMMON::StringHelper::ParseInteger<std::uint32_t>(text);
    if (!parsed || *parsed == 0 || *parsed > maximum)
        return std::nullopt;
    return parsed;
}

auto responseHeaders(std::string_view requestId) -> std::map<std::string, std::string>
{
    return {{"Content-Type", "application/json"},
            {std::string{kRequestIdHeader}, std::string{requestId}},
            {"Cache-Control", "no-store"}};
}
} // namespace

auto PageRequest::offset() const noexcept -> std::uint64_t
{
    return page == 0 ? 0 : static_cast<std::uint64_t>(page - 1) * pageSize;
}

auto RestApi::jsonObject(const HttpRequest &request) -> Result<Json>
{
    try
    {
        const auto contentType = request.header("Content-Type");
        if (!contentType)
            return failure<Json>();
        const auto parts = SZX::COMMON::StringHelper::Split(*contentType, ';');
        if (parts.empty() || parts.size() > 2 ||
            SZX::COMMON::StringHelper::ToLower(SZX::COMMON::StringHelper::Strip(parts.front())) != "application/json")
            return failure<Json>();
        if (parts.size() == 2)
        {
            const auto charset = SZX::COMMON::StringHelper::ToLower(SZX::COMMON::StringHelper::Strip(parts[1]));
            if (charset != "charset=utf-8" && charset != "charset=\"utf-8\"")
                return failure<Json>();
        }
        auto parsed = SZX::COMMON::JsonHelper::ParseRequiredObject(request.body);
        if (!parsed)
            return failure<Json>();
        return Result<Json>::Success(std::move(*parsed));
    } catch (...)
    {
        return failure<Json>(ErrorCode::InternalFailure);
    }
}

auto RestApi::allowedFields(const Json &object, std::span<const std::string_view> fields) -> Result<void>
{
    try
    {
        if (!object.is_object())
            return failure<void>();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (std::find(fields.begin(), fields.end(), it.key()) == fields.end())
                return failure<void>();
        return Result<void>::Success();
    } catch (...)
    {
        return failure<void>(ErrorCode::InternalFailure);
    }
}

auto RestApi::pagination(const HttpRequest &request) -> Result<PageRequest>
{
    try
    {
        PageRequest page;
        for (const auto &[name, maximum, destination] :
             {std::tuple{"page", std::numeric_limits<std::uint32_t>::max(), &page.page},
              std::tuple{"page_size", std::uint32_t{100}, &page.pageSize}})
        {
            const auto found = request.query.find(name);
            if (found == request.query.end())
                continue;
            const auto parsed = positiveInteger(found->second, maximum);
            if (!parsed)
                return failure<PageRequest>();
            *destination = *parsed;
        }
        return Result<PageRequest>::Success(page);
    } catch (...)
    {
        return failure<PageRequest>(ErrorCode::InternalFailure);
    }
}

auto RestApi::ifMatch(const HttpRequest &request) -> Result<std::uint32_t>
{
    try
    {
        const auto header = request.header("If-Match");
        if (!header)
            return failure<std::uint32_t>();
        auto text = std::string_view{*header};
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
            text = text.substr(1, text.size() - 2);
        const auto version = positiveInteger(text, static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()));
        return version ? Result<std::uint32_t>::Success(*version) : failure<std::uint32_t>();
    } catch (...)
    {
        return failure<std::uint32_t>(ErrorCode::InternalFailure);
    }
}

auto RestApi::bearerToken(const HttpRequest &request) -> Result<std::string>
{
    try
    {
        const auto header = request.header("Authorization");
        if (!header || header->size() > 8192)
            return failure<std::string>(ErrorCode::AuthRequired);
        const auto separator = header->find(' ');
        if (separator == std::string_view::npos || SZX::COMMON::StringHelper::ToLower(header->substr(0, separator)) != "bearer")
            return failure<std::string>(ErrorCode::AuthRequired);
        const auto tokenStart = header->find_first_not_of(' ', separator);
        if (tokenStart == std::string_view::npos)
            return failure<std::string>(ErrorCode::AuthRequired);
        const auto token      = header->substr(tokenStart);
        const auto padding    = token.find('=');
        const auto credential = token.substr(0, padding);
        if (credential.empty() ||
            !std::all_of(credential.begin(), credential.end(),
                         [](unsigned char c)
                         {
                             return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                                 c == '.' || c == '_' || c == '~' || c == '+' || c == '/';
                         }) ||
            (padding != std::string_view::npos && token.find_first_not_of('=', padding) != std::string_view::npos))
            return failure<std::string>(ErrorCode::AuthRequired);
        return Result<std::string>::Success(std::string{token});
    } catch (...)
    {
        return failure<std::string>(ErrorCode::InternalFailure);
    }
}

auto RestApi::success(Json data, std::string_view requestId, int statusCode) -> HttpResponse
{
    try
    {
        if (statusCode != 200 && statusCode != 201 && statusCode != 202)
            return error(ErrorCode::InternalFailure, requestId);
        return {.statusCode = statusCode,
                .body       = Json{{"data", std::move(data)}, {"request_id", requestId}}.dump(),
                .headers    = responseHeaders(requestId)};
    } catch (...)
    {
        return error(ErrorCode::InternalFailure, requestId);
    }
}

auto RestApi::noContent(std::string_view requestId) -> HttpResponse
{
    try
    {
        return {.statusCode = 204,
                .headers    = {{std::string{kRequestIdHeader}, std::string{requestId}}, {"Cache-Control", "no-store"}}};
    } catch (...)
    {
        return {.statusCode = 500};
    }
}

auto RestApi::error(ErrorCode code, std::string_view requestId) -> HttpResponse
{
    try
    {
        switch (code)
        {
        case ErrorCode::AuthRequired:
        case ErrorCode::TokenExpired:
        case ErrorCode::TokenReplayed:
        case ErrorCode::AuthInvalidCredentials:
        case ErrorCode::Forbidden:
        case ErrorCode::ResourceNotFound:
        case ErrorCode::ValidationError:
        case ErrorCode::UserIdentityOneOfRequired:
        case ErrorCode::UserIdentityConflict:
        case ErrorCode::ConflictVersion:
        case ErrorCode::LastSuperAdminProtected:
        case ErrorCode::RateLimited:
        case ErrorCode::AuditWriteFailed:
        case ErrorCode::RequestIdInvalid:
        case ErrorCode::RouteConflict:
        case ErrorCode::PayloadTooLarge:
        case ErrorCode::ServiceUnavailable:
        case ErrorCode::TransportFailure:
        case ErrorCode::InvalidConfiguration:
        case ErrorCode::MethodNotAllowed:
        case ErrorCode::DatabaseUnavailable:
        case ErrorCode::DatabaseQueryFailed:
        case ErrorCode::DatabaseTransactionFailed:
        case ErrorCode::MigrationFailed:
        case ErrorCode::MigrationConflict:
        case ErrorCode::InternalFailure:
            break;
        default:
            code = ErrorCode::InternalFailure;
        }
        auto headers      = responseHeaders(requestId);
        const auto status = errorCodeHttpStatus(code);
        if (status == 401)
            headers.emplace("WWW-Authenticate", "Bearer");
        return {.statusCode = status,
                .body       = Json{{"code", static_cast<int>(code)},
                                   {"message", defaultErrorMessage(code)},
                                   {"details", Json::object()},
                                   {"request_id", requestId}}
                            .dump(),
                .headers = std::move(headers)};
    } catch (...)
    {
        return {.statusCode = 500};
    }
}
} // namespace SZX::ADMIN::INFRA
