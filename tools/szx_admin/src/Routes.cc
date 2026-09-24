#include "Routes.h"
#include "HttpSupport.h"
#include "TimeHelper.hpp"
#include "auth/LoginService.h"
#include "rbac/RbacHandler.h"
#include "telemetry/BehaviorEventWriter.h"
#include "user/UserHandler.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RestApi.h"

#include <algorithm>
#include <array>
#include <string>

namespace SZX::ADMIN
{
namespace
{
auto refreshCookie(const std::string &path, std::string_view token, std::int64_t maxAge) -> std::string
{
    return "szx_refresh=" + std::string{token} + "; Path=" + path + "; Max-Age=" + std::to_string(maxAge) +
        "; HttpOnly; Secure; SameSite=None";
}

auto sessionResponse(const AUTH::LoginSession &session, const INFRA::HttpRequest &request, const std::string &cookiePath)
    -> INFRA::HttpResponse
{
    using INFRA::ErrorCode;
    using INFRA::RestApi;
    using SZX::COMMON::Json;
    try
    {
        const auto &user = session.user;
        Json profile{{"id", user.id},
                     {"display_name", user.display_name},
                     {"role", user.role},
                     {"status", user.status},
                     {"force_password_change", user.force_password_change && user.role == "super_admin"},
                     {"password_change_recommended", user.force_password_change && user.role != "super_admin"},
                     {"version", user.version},
                     {"authz_version", user.authz_version},
                     {"last_login_at", utcTimestampJson(user.last_login_at)},
                     {"created_at", utcTimestampJson(user.created_at)},
                     {"updated_at", utcTimestampJson(user.updated_at)}};
        if (user.uid)
        {
            profile["uid"] = *user.uid;
            profile["login_identifier"] = *user.uid;
            profile["identifier_type"] = "uid";
        }
        if (user.username)
        {
            profile["username"] = *user.username;
            profile["login_identifier"] = *user.username;
            profile["identifier_type"] = "username";
        }
        auto response = RestApi::success(Json{{"user", std::move(profile)},
                                              {"access_token", session.accessToken},
                                              {"token_type", "Bearer"},
                                              {"expires_in", 900},
                                              {"expires_at", session.expiresAt * 1000},
                                              {"authz_version", user.authz_version},
                                              {"csrf_token", session.csrfToken}},
                                         request.requestId);
        if (response.statusCode == 200)
            response.headers.emplace(
                "Set-Cookie",
                refreshCookie(
                    cookiePath, session.refreshToken,
                    (std::max)(std::int64_t{0}, session.refreshExpiresAt - SZX::COMMON::TimeHelper::NowTimestamp() / 1000)));
        return response;
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}
auto loginResponse(AUTH::LoginService &service, const INFRA::HttpRequest &request, const std::string &cookiePath)
    -> INFRA::HttpResponse
{
    using INFRA::ErrorCode;
    using INFRA::RestApi;
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 4> fields{"identifier", "login_identifier", "identifier_type", "password_hash"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("password_hash") ||
            !body.value.at("password_hash").is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const bool legacy = body.value.contains("identifier");
        const bool formal = body.value.contains("login_identifier") || body.value.contains("identifier_type");
        if (legacy == formal)
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        AUTH::LoginInput input{.identifier = {},
                               .passwordHash = body.value.at("password_hash").get<std::string>(),
                               .requestId = request.requestId,
                               .peerAddress = request.peerAddress};
        if (legacy)
        {
            if (!body.value.at("identifier").is_string())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
            input.identifier = body.value.at("identifier").get<std::string>();
        }
        else
        {
            if (!body.value.at("login_identifier").is_string() || !body.value.at("identifier_type").is_string())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
            input.identifier = body.value.at("login_identifier").get<std::string>();
            const auto type = body.value.at("identifier_type").get<std::string>();
            if (type == "username")
                input.identifierType = AUTH::IdentifierType::Username;
            else if (type == "uid")
                input.identifierType = AUTH::IdentifierType::Uid;
            else
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
        }
        const auto session = service.login(input);
        return session.ok ? sessionResponse(session.value, request, cookiePath) :
                            RestApi::error(static_cast<ErrorCode>(session.error), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

// Cookie pairs allow only HTTP optional whitespace around the name/value delimiter.
auto trimCookieWhitespace(std::string_view value) -> std::string_view
{
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string_view::npos)
        return {};
    return value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

auto cookieSessionResponse(AUTH::LoginService &service, const INFRA::HttpRequest &request, const std::string &cookiePath,
                           bool logout) -> INFRA::HttpResponse
{
    using INFRA::ErrorCode;
    using INFRA::RestApi;
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        if (!request.body.empty())
        {
            const auto body = RestApi::jsonObject(request);
            if (!body.ok || !body.value.empty())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
        }
        std::optional<std::string> token;
        if (const auto cookie = request.header("Cookie"))
        {
            if (cookie->size() > 8192)
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
            auto remaining = *cookie;
            while (!remaining.empty())
            {
                const auto end   = remaining.find(';');
                const auto pair  = trimCookieWhitespace(remaining.substr(0, end));
                const auto equal = pair.find('=');
                if (trimCookieWhitespace(pair.substr(0, equal)) == "szx_refresh")
                {
                    if (token || equal == std::string_view::npos)
                        return RestApi::error(ErrorCode::ValidationError, request.requestId);
                    token = trimCookieWhitespace(pair.substr(equal + 1));
                }
                if (end == std::string_view::npos)
                    break;
                remaining.remove_prefix(end + 1);
            }
        }
        if (!token && !logout)
            return RestApi::error(ErrorCode::AuthRequired, request.requestId);
        if (token)
        {
            const AUTH::CookieSessionInput input{.refreshToken = *token,
                                                 .csrfToken =
                                                     std::string{request.header("X-CSRF-Token").value_or(std::string_view{})},
                                                 .requestId   = request.requestId,
                                                 .peerAddress = request.peerAddress};
            if (!logout)
            {
                const auto session = service.refresh(input);
                return session.ok ? sessionResponse(session.value, request, cookiePath) :
                                    RestApi::error(static_cast<ErrorCode>(session.error), request.requestId);
            }
            const auto result = service.logout(input);
            if (!result.ok)
                return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        }
        auto response = RestApi::noContent(request.requestId);
        response.headers.emplace("Set-Cookie", refreshCookie(cookiePath, {}, 0));
        return response;
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto changePasswordResponse(AUTH::LoginService &service, const INFRA::HttpRequest &request, const std::string &cookiePath)
    -> INFRA::HttpResponse
{
    using INFRA::ErrorCode;
    using INFRA::RestApi;
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto token = RestApi::bearerToken(request);
        if (!token.ok)
            return RestApi::error(static_cast<ErrorCode>(token.error), request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 2> fields{"old_password_hash", "new_password_hash"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains(fields[0]) ||
            !body.value.contains(fields[1]) || !body.value.at(fields[0]).is_string() || !body.value.at(fields[1]).is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto result = service.changePassword({.accessToken     = token.value,
                                                    .oldPasswordHash = body.value.at(fields[0]).get<std::string>(),
                                                    .newPasswordHash = body.value.at(fields[1]).get<std::string>(),
                                                    .version         = version.value,
                                                    .requestId       = request.requestId,
                                                    .peerAddress     = request.peerAddress});
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        auto response = RestApi::success(SZX::COMMON::Json{{"version", result.value.version},
                                                           {"authz_version", result.value.authzVersion},
                                                           {"force_password_change", false},
                                                           {"password_change_recommended", false},
                                                           {"reauthentication_required", true}},
                                         request.requestId);
        if (response.statusCode == 200)
            response.headers.emplace("Set-Cookie", refreshCookie(cookiePath, {}, 0));
        return response;
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}
} // namespace
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

auto registerRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database, AUTH::LoginService *login,
                    TELEMETRY::BehaviorEventWriter *writer)
    -> SZX::COMMON::Result<void>
{
    try
    {
        const auto ready = transport.registerApiRoute(
            "GET", "/ready",
            [&database, writer](const INFRA::HttpRequest &request)
            {
                const auto ping = database.ping();
                if (!ping.ok)
                    return INFRA::RestApi::error(INFRA::ErrorCode::DatabaseUnavailable, request.requestId);
                const auto rbac = database.withConnection(
                    [](INFRA::DatabaseConnection &connection) -> SZX::COMMON::Result<void>
                    {
                        const auto ready = connection.rbacCatalogReady();
                        if (!ready.ok)
                            return SZX::COMMON::Result<void>::Failure(ready.error, {});
                        return ready.value ? SZX::COMMON::Result<void>::Success() :
                                             SZX::COMMON::Result<void>::Failure(
                                                 static_cast<int>(INFRA::ErrorCode::ServiceUnavailable), {});
                    });
                if (!rbac.ok)
                    return INFRA::RestApi::error(rbac.error == static_cast<int>(INFRA::ErrorCode::ServiceUnavailable) ?
                                                     INFRA::ErrorCode::ServiceUnavailable : INFRA::ErrorCode::DatabaseUnavailable,
                                                 request.requestId);
                if (writer && writer->state() != TELEMETRY::AdmissionState::Accepting)
                    return INFRA::RestApi::error(INFRA::ErrorCode::ServiceUnavailable, request.requestId);
                return INFRA::RestApi::success(SZX::COMMON::Json{{"status", "ready"}}, request.requestId);
            });
        if (!ready.ok || !login)
            return ready;
        const auto &prefix    = transport.config().apiPrefix;
        const auto cookiePath = prefix == "/" ? std::string{"/auth"} : prefix + "/auth";
        const auto post =
            transport.registerApiRoute("POST", "/auth/login",
                                       [login, cookiePath](const INFRA::HttpRequest &request)
                                       {
                                           if (!originAllowed(login->config().allowedOrigin, request))
                                               return INFRA::RestApi::error(INFRA::ErrorCode::Forbidden, request.requestId);
                                           return withCors(loginResponse(*login, request, cookiePath), request);
                                       });
        if (!post.ok)
            return post;
        const auto change =
            transport.registerApiRoute("POST", "/auth/change-password",
                                       [login, cookiePath](const INFRA::HttpRequest &request)
                                       {
                                           if (!originAllowed(login->config().allowedOrigin, request))
                                               return INFRA::RestApi::error(INFRA::ErrorCode::Forbidden, request.requestId);
                                           return withCors(changePasswordResponse(*login, request, cookiePath), request);
                                       });
        if (!change.ok)
            return change;
        for (const auto logout : {false, true})
        {
            const auto registered = transport.registerApiRoute(
                "POST", logout ? "/auth/logout" : "/auth/refresh",
                [login, cookiePath, logout](const INFRA::HttpRequest &request)
                {
                    if (!originAllowed(login->config().allowedOrigin, request))
                        return INFRA::RestApi::error(INFRA::ErrorCode::Forbidden, request.requestId);
                    return withCors(cookieSessionResponse(*login, request, cookiePath, logout), request);
                });
            if (!registered.ok)
                return registered;
        }
        const auto preflight = preflightHandler(login->config().allowedOrigin, "POST, OPTIONS");
        for (const auto path : {"/auth/login", "/auth/change-password", "/auth/refresh", "/auth/logout"})
        {
            const auto options = transport.registerApiRoute("OPTIONS", path, preflight);
            if (!options.ok)
                return options;
        }
        if (!writer)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(INFRA::ErrorCode::InvalidConfiguration), {});
        const auto userRoutes = USER::registerUserRoutes(transport, database, *login, *writer);
        if (!userRoutes.ok)
            return userRoutes;
        const auto rbacRoutes = RBAC::registerRbacRoutes(transport, database, *login);
        if (!rbacRoutes.ok)
            return rbacRoutes;
        return SZX::COMMON::Result<void>::Success();
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN
