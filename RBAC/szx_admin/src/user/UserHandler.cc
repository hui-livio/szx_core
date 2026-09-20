#include "UserHandler.h"
#include "../HttpSupport.h"
#include "../auth/LoginService.h"
#include "AuditDetails.h"
#include "UserService.h"
#include "UserValidation.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/RestApi.h"
#include "SzxAdmin/Policy/Policy.h"
#include "TimeHelper.hpp"

#include <array>

namespace SZX::ADMIN::USER
{
namespace
{
using INFRA::ErrorCode;
using INFRA::RestApi;
using SZX::COMMON::Json;

auto actorContext(const INFRA::HttpRequest &request) -> ActorContext
{
    const auto token = RestApi::bearerToken(request);
    return {.accessToken = token.ok ? token.value : std::string{},
            .requestId   = request.requestId,
            .peerAddress = request.peerAddress};
}

auto permissionsResponse(UserService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    try
    {
        const auto result = service.permissions(actorContext(request));
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        Json capabilities = Json::array();
        for (const auto capability : result.value.capabilities)
            capabilities.push_back(std::string{capability});
        return RestApi::success(Json{{"user_id", result.value.userId},
                                     {"role", result.value.role},
                                     {"status", result.value.status},
                                     {"force_password_change", result.value.forcePasswordChange},
                                     {"authz_version", result.value.authzVersion},
                                     {"capabilities", std::move(capabilities)}},
                                request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto userRowJson(const UserRow &row) -> Json
{
    Json profile{{"id", row.id},
                 {"display_name", row.display_name},
                 {"role", row.role},
                 {"status", row.status},
                 {"force_password_change", row.force_password_change && row.role == "super_admin"},
                 {"password_change_recommended", row.force_password_change && row.role != "super_admin"},
                 {"version", row.version},
                 {"authz_version", row.authz_version},
                 {"last_login_at", utcTimestampJson(row.last_login_at)},
                 {"created_at", utcTimestampJson(row.created_at)},
                 {"updated_at", utcTimestampJson(row.updated_at)}};
    if (row.uid)
        profile["uid"] = *row.uid;
    if (row.username)
        profile["username"] = *row.username;
    return profile;
}

auto listResponse(UserService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    try
    {
        constexpr std::array<std::string_view, 6> allowed{"page", "page_size", "role", "status", "search", "sort"};
        if (!allowedQuery(request, allowed))
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto pagination = RestApi::pagination(request);
        if (!pagination.ok)
            return RestApi::error(static_cast<ErrorCode>(pagination.error), request.requestId);
        const ListQuery query{.role     = queryValue(request, "role"),
                              .status   = queryValue(request, "status"),
                              .search   = queryValue(request, "search"),
                              .sort     = queryValue(request, "sort"),
                              .page     = pagination.value.page,
                              .pageSize = pagination.value.pageSize};
        const auto result = service.list(actorContext(request), query);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        Json items = Json::array();
        for (const auto &row : result.value.items)
            items.push_back(userRowJson(row));
        return RestApi::success(
            Json{{"items", std::move(items)}, {"total", result.value.total}, {"page", query.page}, {"page_size", query.pageSize}},
            request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto getResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        const auto result = service.get(actorContext(request), id);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(userRowJson(result.value), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto createResponse(UserService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 5> fields{"uid", "username", "display_name", "role", "password_hash"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("display_name") ||
            !body.value.at("display_name").is_string() || !body.value.contains("role") || !body.value.at("role").is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const bool hasUid      = body.value.contains("uid") && !body.value.at("uid").is_null();
        const bool hasUsername = body.value.contains("username") && !body.value.at("username").is_null();
        if (hasUid == hasUsername)
            return RestApi::error(ErrorCode::UserIdentityOneOfRequired, request.requestId);
        if ((hasUid && !body.value.at("uid").is_string()) || (hasUsername && !body.value.at("username").is_string()))
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        CreateUserInput input{.displayName = body.value.at("display_name").get<std::string>(),
                              .role        = body.value.at("role").get<std::string>()};
        if (hasUid)
            input.uid = body.value.at("uid").get<std::string>();
        else
            input.username = body.value.at("username").get<std::string>();
        if (body.value.contains("password_hash"))
        {
            if (!body.value.at("password_hash").is_string())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
            input.passwordHash = body.value.at("password_hash").get<std::string>();
        }
        const auto result = service.create(actorContext(request), input);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(
            Json{{"user", userRowJson(result.value.user)},
                 {"temporary_credential", input.passwordHash ? Json(nullptr) : Json(result.value.temporaryCredential)},
                 {"temporary_credential_expires_at",
                  input.passwordHash ? Json(nullptr) : utcTimestampJson(result.value.temporaryCredentialExpiresAt)}},
            request.requestId, 201);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto updateProfileResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 1> fields{"display_name"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("display_name") ||
            !body.value.at("display_name").is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto result =
            service.updateProfile(actorContext(request), id, body.value.at("display_name").get<std::string>(), version.value);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(userRowJson(result.value), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto changeStatusResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 1> fields{"status"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("status") ||
            !body.value.at("status").is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto result =
            service.changeStatus(actorContext(request), id, body.value.at("status").get<std::string>(), version.value);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(userRowJson(result.value), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto unlockResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        if (!request.body.empty())
        {
            const auto body = RestApi::jsonObject(request);
            if (!body.ok || !body.value.empty())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
        }
        const auto result = service.unlock(actorContext(request), id, version.value);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(userRowJson(result.value), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto assignRoleResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 1> fields{"role"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("role") || !body.value.at("role").is_string())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto result =
            service.assignRole(actorContext(request), id, body.value.at("role").get<std::string>(), version.value);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(userRowJson(result.value), request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto resetPasswordResponse(UserService &service, const INFRA::HttpRequest &request, const std::string &id) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto version = RestApi::ifMatch(request);
        if (!version.ok)
            return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
        if (!request.body.empty())
        {
            const auto body = RestApi::jsonObject(request);
            if (!body.ok || !body.value.empty())
                return RestApi::error(ErrorCode::ValidationError, request.requestId);
        }
        const auto result = service.resetPassword(actorContext(request), id, version.value);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(
            Json{{"user", userRowJson(result.value.user)},
                 {"temporary_credential", result.value.temporaryCredential},
                 {"temporary_credential_expires_at", utcTimestampJson(result.value.temporaryCredentialExpiresAt)}},
            request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto logsResponse(UserService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    try
    {
        constexpr std::array<std::string_view, 8> allowed{"page",       "page_size", "from",          "to",
                                                          "event_name", "result",    "actor_user_id", "actor_search"};
        if (!allowedQuery(request, allowed))
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const auto pagination = RestApi::pagination(request);
        if (!pagination.ok)
            return RestApi::error(static_cast<ErrorCode>(pagination.error), request.requestId);
        const LogQuery query{.from        = queryValue(request, "from"),
                             .to          = queryValue(request, "to"),
                             .eventName   = queryValue(request, "event_name"),
                             .result      = queryValue(request, "result"),
                             .actorUserId = queryValue(request, "actor_user_id"),
                             .page        = pagination.value.page,
                             .pageSize    = pagination.value.pageSize,
                             .actorSearch = queryValue(request, "actor_search")};
        const auto result = service.logs(actorContext(request), query);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        Json items = Json::array();
        for (const auto &entry : result.value.items)
        {
            auto details = SZX::COMMON::JsonHelper::ParseRequiredObject(entry.details_json);
            if (!details)
                return RestApi::error(ErrorCode::DatabaseQueryFailed, request.requestId);
            redactAuditDetails(*details);
            items.push_back(Json{{"id", entry.id},
                                 {"event_name", entry.event_name},
                                 {"result", entry.result},
                                 {"actor_user_id", entry.actor_user_id.value_or("")},
                                 {"actor_username", entry.actor_username},
                                 {"target_user_id", entry.target_user_id.value_or("")},
                                 {"request_id", entry.request_id},
                                 {"peer_address", ""},
                                 {"details", std::move(*details)},
                                 {"created_at", utcTimestampJson(entry.created_at)}});
        }
        return RestApi::success(
            Json{{"items", std::move(items)}, {"total", result.value.total}, {"page", query.page}, {"page_size", query.pageSize}},
            request.requestId);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}

auto recordBehaviorResponse(UserService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    try
    {
        if (request.body.size() > 8192)
            return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
        const auto body = RestApi::jsonObject(request);
        if (!body.ok)
            return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
        constexpr std::array<std::string_view, 3> fields{"event_name", "result", "details"};
        if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("event_name") ||
            !body.value.at("event_name").is_string() || !body.value.contains("result") || !body.value.at("result").is_string() ||
            !body.value.contains("details") || !body.value.at("details").is_object())
            return RestApi::error(ErrorCode::ValidationError, request.requestId);
        const BehaviorEvent event{.eventName   = body.value.at("event_name").get<std::string>(),
                                  .result      = body.value.at("result").get<std::string>(),
                                  .detailsJson = body.value.at("details").dump()};
        const auto result = service.recordBehavior(actorContext(request), event);
        if (!result.ok)
            return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
        return RestApi::success(Json{{"event_id", result.value}}, request.requestId, 202);
    } catch (...)
    {
        return RestApi::error(ErrorCode::InternalFailure, request.requestId);
    }
}
} // namespace

auto registerUserRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database, AUTH::LoginService &login)
    -> SZX::COMMON::Result<void>
{
    try
    {
        auto service             = std::make_shared<UserService>(database, login);
        const auto allowedOrigin = login.config().allowedOrigin;

        const auto permissions =
            transport.registerApiRoute("GET", "/client/me/permissions",
                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                       {
                                           if (!originAllowed(allowedOrigin, request))
                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                           return withCors(permissionsResponse(*service, request), request);
                                       });
        if (!permissions.ok)
            return permissions;

        const auto list = transport.registerApiRoute("GET", "/admin/users",
                                                     [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                     {
                                                         if (!originAllowed(allowedOrigin, request))
                                                             return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                         return withCors(listResponse(*service, request), request);
                                                     });
        if (!list.ok)
            return list;

        const auto get =
            transport.registerApiRoute("GET", "/admin/users/{id}",
                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                       {
                                           if (!originAllowed(allowedOrigin, request))
                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                           return withCors(getResponse(*service, request, request.pathParams.at("id")), request);
                                       });
        if (!get.ok)
            return get;

        const auto create = transport.registerApiRoute("POST", "/admin/users",
                                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                       {
                                                           if (!originAllowed(allowedOrigin, request))
                                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                           return withCors(createResponse(*service, request), request);
                                                       });
        if (!create.ok)
            return create;

        const auto update = transport.registerApiRoute("PATCH", "/admin/users/{id}",
                                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                       {
                                                           if (!originAllowed(allowedOrigin, request))
                                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                           const auto id = request.pathParams.at("id");
                                                           return withCors(updateProfileResponse(*service, request, id), request);
                                                       });
        if (!update.ok)
            return update;

        const auto status = transport.registerApiRoute("PATCH", "/admin/users/{id}/status",
                                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                       {
                                                           if (!originAllowed(allowedOrigin, request))
                                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                           const auto id = request.pathParams.at("id");
                                                           return withCors(changeStatusResponse(*service, request, id), request);
                                                       });
        if (!status.ok)
            return status;

        const auto unlock = transport.registerApiRoute("PATCH", "/admin/users/{id}/unlock",
                                                       [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                       {
                                                           if (!originAllowed(allowedOrigin, request))
                                                               return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                           const auto id = request.pathParams.at("id");
                                                           return withCors(unlockResponse(*service, request, id), request);
                                                       });
        if (!unlock.ok)
            return unlock;

        const auto role = transport.registerApiRoute("PUT", "/admin/users/{id}/role",
                                                     [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                     {
                                                         if (!originAllowed(allowedOrigin, request))
                                                             return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                         const auto id = request.pathParams.at("id");
                                                         return withCors(assignRoleResponse(*service, request, id), request);
                                                     });
        if (!role.ok)
            return role;

        const auto reset = transport.registerApiRoute("POST", "/admin/users/{id}/reset-password",
                                                      [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                      {
                                                          if (!originAllowed(allowedOrigin, request))
                                                              return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                          const auto id = request.pathParams.at("id");
                                                          return withCors(resetPasswordResponse(*service, request, id), request);
                                                      });
        if (!reset.ok)
            return reset;

        const auto logs = transport.registerApiRoute("GET", "/admin/user-logs",
                                                     [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                     {
                                                         if (!originAllowed(allowedOrigin, request))
                                                             return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                         return withCors(logsResponse(*service, request), request);
                                                     });
        if (!logs.ok)
            return logs;

        const auto behavior = transport.registerApiRoute("POST", "/client/behavior-events",
                                                         [service, allowedOrigin](const INFRA::HttpRequest &request)
                                                         {
                                                             if (!originAllowed(allowedOrigin, request))
                                                                 return RestApi::error(ErrorCode::Forbidden, request.requestId);
                                                             return withCors(recordBehaviorResponse(*service, request), request);
                                                         });
        if (!behavior.ok)
            return behavior;

        const auto preflight = preflightHandler(allowedOrigin, "GET, POST, PUT, PATCH, OPTIONS");
        for (const auto path : {"/client/me/permissions", "/admin/users", "/admin/users/{id}", "/admin/users/{id}/status",
                                "/admin/users/{id}/unlock", "/admin/users/{id}/role", "/admin/users/{id}/reset-password",
                                "/admin/user-logs", "/client/behavior-events"})
        {
            const auto options = transport.registerApiRoute("OPTIONS", path, preflight);
            if (!options.ok)
                return options;
        }

        return SZX::COMMON::Result<void>::Success();
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN::USER
