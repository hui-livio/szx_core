#include "RbacHandler.h"

#include "../HttpSupport.h"
#include "../auth/LoginService.h"
#include "../user/ActorContext.h"
#include "PermissionDto.h"
#include "PermissionService.h"
#include "RoleDto.h"
#include "RoleService.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/RestApi.h"

#include <array>

namespace SZX::ADMIN::RBAC
{
namespace
{
using INFRA::ErrorCode;
using INFRA::RestApi;
using SZX::COMMON::Json;

auto actorContext(const INFRA::HttpRequest &request) -> USER::ActorContext
{
    const auto token = RestApi::bearerToken(request);
    const auto agent = request.header("User-Agent");
    return {.accessToken = token.ok ? token.value : std::string{},
            .requestId   = request.requestId,
            .peerAddress = request.peerAddress,
            .userAgent   = agent ? std::string{*agent} : std::string{}};
}

auto includeArchived(const INFRA::HttpRequest &request) -> std::optional<bool>
{
    const auto value = queryValue(request, "include_archived");
    if (value.empty() || value == "false")
        return false;
    if (value == "true")
        return true;
    return std::nullopt;
}

auto roleDto(const RoleRow &row) -> RoleDto
{
    return {row.id,      row.code,         row.name,      row.description,     row.systemKey, row.status,
            row.version, row.authzVersion, row.userCount, row.permissionCount, row.manageable};
}
auto roleResponse(const RoleRow &row, std::string_view requestId, int status = 200) -> INFRA::HttpResponse
{
    std::string body;
    SZX::COMMON::Serializer::to_json(RoleResponse{roleDto(row), std::string{requestId}}, body);
    return RestApi::serializedSuccess(std::move(body), requestId, status);
}

auto permissionDto(const PermissionRow &row) -> PermissionDto
{
    return {row.id,          row.code,   row.name,   row.description,           row.module,
            row.enforcement, row.source, row.status, row.requiredForSuperAdmin, row.version};
}
auto permissionResponse(const PermissionRow &row, std::string_view requestId, int status = 200) -> INFRA::HttpResponse
{
    std::string body;
    SZX::COMMON::Serializer::to_json(PermissionResponse{permissionDto(row), std::string{requestId}}, body);
    return RestApi::serializedSuccess(std::move(body), requestId, status);
}

auto roleListResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    if (!allowedQuery(request, std::array<std::string_view, 1>{"include_archived"}))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto archived = includeArchived(request);
    if (!archived)
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result = service.list(actorContext(request), *archived);
    if (!result.ok)
        return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
    RoleListResponse response;
    response.request_id = request.requestId;
    for (const auto &row : result.value)
        response.data.items.push_back(roleDto(row));
    std::string body;
    SZX::COMMON::Serializer::to_json(response, body);
    return RestApi::serializedSuccess(std::move(body), request.requestId);
}

auto roleGetResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto result = service.get(actorContext(request), request.pathParams.at("id"));
    return result.ok ? roleResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto roleCreateResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    if (request.body.size() > 65536)
        return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok)
        return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
    constexpr std::array<std::string_view, 3> fields{"code", "name", "description"};
    if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("code") || !body.value.contains("name") ||
        !body.value.at("code").is_string() || !body.value.at("name").is_string() ||
        (body.value.contains("description") && !body.value.at("description").is_string()))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result =
        service.create(actorContext(request),
                       NewRoleInput{body.value.at("code").get<std::string>(), body.value.at("name").get<std::string>(),
                                    body.value.value("description", "")});
    return result.ok ? roleResponse(result.value, request.requestId, 201) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto roleUpdateResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok)
        return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
    constexpr std::array<std::string_view, 2> fields{"name", "description"};
    if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("name") || !body.value.at("name").is_string() ||
        (body.value.contains("description") && !body.value.at("description").is_string()))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result =
        service.updateProfile(actorContext(request), request.pathParams.at("id"), body.value.at("name").get<std::string>(),
                              body.value.value("description", ""), version.value);
    return result.ok ? roleResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto roleStatusResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok || !body.value.contains("status") || !body.value.at("status").is_string() ||
        !RestApi::allowedFields(body.value, std::array<std::string_view, 1>{"status"}).ok ||
        (body.value.at("status") != "active" && body.value.at("status") != "disabled"))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result = service.changeStatus(actorContext(request), request.pathParams.at("id"),
                                             body.value.at("status").get<std::string>(), version.value);
    return result.ok ? roleResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto roleArchiveResponse(RoleService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto result = service.changeStatus(actorContext(request), request.pathParams.at("id"), "archived", version.value);
    return result.ok ? RestApi::noContent(request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto stringArray(const Json &body, std::string_view field) -> std::optional<std::vector<std::string>>
{
    if (!body.contains(field) || !body.at(field).is_array() || body.at(field).size() > 1000)
        return std::nullopt;
    std::vector<std::string> values;
    for (const auto &value : body.at(field))
    {
        if (!value.is_string())
            return std::nullopt;
        values.push_back(value.get<std::string>());
    }
    return values;
}

auto roleCollectionGet(RoleService &service, const INFRA::HttpRequest &request, bool scopes) -> INFRA::HttpResponse
{
    const auto result = scopes ? service.manageScopeIds(actorContext(request), request.pathParams.at("id")) :
                                 service.permissionIds(actorContext(request), request.pathParams.at("id"));
    if (!result.ok)
        return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
    Json values = Json::array();
    for (const auto &value : result.value)
        values.push_back(value);
    return RestApi::success(Json{{scopes ? "target_role_ids" : "permission_ids", std::move(values)}}, request.requestId);
}

auto roleCollectionPut(RoleService &service, const INFRA::HttpRequest &request, bool scopes) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok)
        return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
    const std::array<std::string_view, 1> fields{scopes ? "target_role_ids" : "permission_ids"};
    if (!RestApi::allowedFields(body.value, fields).ok)
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto values = stringArray(body.value, fields.front());
    if (!values)
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result = scopes ?
        service.replaceManageScopeIds(actorContext(request), request.pathParams.at("id"), *values, version.value) :
        service.replacePermissionIds(actorContext(request), request.pathParams.at("id"), *values, version.value);
    return result.ok ? roleResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto permissionListResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    if (!allowedQuery(request, std::array<std::string_view, 1>{"include_archived"}))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto archived = includeArchived(request);
    if (!archived)
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result = service.list(actorContext(request), *archived);
    if (!result.ok)
        return RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
    PermissionListResponse response;
    response.request_id = request.requestId;
    for (const auto &row : result.value)
        response.data.items.push_back(permissionDto(row));
    std::string body;
    SZX::COMMON::Serializer::to_json(response, body);
    return RestApi::serializedSuccess(std::move(body), request.requestId);
}

auto permissionGetResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto result = service.get(actorContext(request), request.pathParams.at("id"));
    return result.ok ? permissionResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto permissionCreateResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    if (request.body.size() > 65536)
        return RestApi::error(ErrorCode::PayloadTooLarge, request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok)
        return RestApi::error(static_cast<ErrorCode>(body.error), request.requestId);
    constexpr std::array<std::string_view, 4> fields{"code", "name", "description", "module"};
    if (!RestApi::allowedFields(body.value, fields).ok || !body.value.contains("code") || !body.value.contains("name") ||
        !body.value.contains("module") || !body.value.at("code").is_string() || !body.value.at("name").is_string() ||
        !body.value.at("module").is_string() || (body.value.contains("description") && !body.value.at("description").is_string()))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result =
        service.create(actorContext(request),
                       NewPermissionInput{body.value.at("code").get<std::string>(), body.value.at("name").get<std::string>(),
                                          body.value.value("description", ""), body.value.at("module").get<std::string>()});
    return result.ok ? permissionResponse(result.value, request.requestId, 201) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto permissionUpdateResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok || !RestApi::allowedFields(body.value, std::array<std::string_view, 2>{"name", "description"}).ok ||
        !body.value.contains("name") || !body.value.at("name").is_string() ||
        (body.value.contains("description") && !body.value.at("description").is_string()))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result =
        service.updateProfile(actorContext(request), request.pathParams.at("id"), body.value.at("name").get<std::string>(),
                              body.value.value("description", ""), version.value);
    return result.ok ? permissionResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto permissionStatusResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto body = RestApi::jsonObject(request);
    if (!body.ok || !RestApi::allowedFields(body.value, std::array<std::string_view, 1>{"status"}).ok ||
        !body.value.contains("status") || !body.value.at("status").is_string() ||
        (body.value.at("status") != "active" && body.value.at("status") != "disabled"))
        return RestApi::error(ErrorCode::ValidationError, request.requestId);
    const auto result = service.changeStatus(actorContext(request), request.pathParams.at("id"),
                                             body.value.at("status").get<std::string>(), version.value);
    return result.ok ? permissionResponse(result.value, request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}

auto permissionArchiveResponse(PermissionService &service, const INFRA::HttpRequest &request) -> INFRA::HttpResponse
{
    const auto version = RestApi::ifMatch(request);
    if (!version.ok)
        return RestApi::error(static_cast<ErrorCode>(version.error), request.requestId);
    const auto result = service.changeStatus(actorContext(request), request.pathParams.at("id"), "archived", version.value);
    return result.ok ? RestApi::noContent(request.requestId) :
                       RestApi::error(static_cast<ErrorCode>(result.error), request.requestId);
}
} // namespace

auto registerRbacRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database, AUTH::LoginService &login)
    -> SZX::COMMON::Result<void>
{
    try
    {
        auto roles        = std::make_shared<RoleService>(database, login);
        auto permissions  = std::make_shared<PermissionService>(database, login);
        const auto origin = login.config().allowedOrigin;
        const auto bind   = [&](std::string method, std::string path, INFRA::HttpHandler handler)
        {
            return transport.registerApiRoute(std::move(method), std::move(path),
                                              [handler = std::move(handler)](const INFRA::HttpRequest &request)
                                              {
                                                  try
                                                  {
                                                      return handler(request);
                                                  } catch (...)
                                                  {
                                                      return RestApi::error(ErrorCode::InternalFailure, request.requestId);
                                                  }
                                              });
        };

        const auto roleList = bind("GET", "/admin/roles",
                                   [roles, origin](const INFRA::HttpRequest &request)
                                   {
                                       return originAllowed(origin, request) ?
                                           withCors(roleListResponse(*roles, request), request) :
                                           RestApi::error(ErrorCode::Forbidden, request.requestId);
                                   });
        if (!roleList.ok)
            return roleList;
        const auto roleGet = bind("GET", "/admin/roles/{id}",
                                  [roles, origin](const INFRA::HttpRequest &request)
                                  {
                                      return originAllowed(origin, request) ?
                                          withCors(roleGetResponse(*roles, request), request) :
                                          RestApi::error(ErrorCode::Forbidden, request.requestId);
                                  });
        if (!roleGet.ok)
            return roleGet;
        const auto roleCreate = bind("POST", "/admin/roles",
                                     [roles, origin](const INFRA::HttpRequest &request)
                                     {
                                         return originAllowed(origin, request) ?
                                             withCors(roleCreateResponse(*roles, request), request) :
                                             RestApi::error(ErrorCode::Forbidden, request.requestId);
                                     });
        if (!roleCreate.ok)
            return roleCreate;
        const auto roleUpdate = bind("PATCH", "/admin/roles/{id}",
                                     [roles, origin](const INFRA::HttpRequest &request)
                                     {
                                         return originAllowed(origin, request) ?
                                             withCors(roleUpdateResponse(*roles, request), request) :
                                             RestApi::error(ErrorCode::Forbidden, request.requestId);
                                     });
        if (!roleUpdate.ok)
            return roleUpdate;
        const auto roleStatus = bind("PATCH", "/admin/roles/{id}/status",
                                     [roles, origin](const INFRA::HttpRequest &request)
                                     {
                                         return originAllowed(origin, request) ?
                                             withCors(roleStatusResponse(*roles, request), request) :
                                             RestApi::error(ErrorCode::Forbidden, request.requestId);
                                     });
        if (!roleStatus.ok)
            return roleStatus;
        const auto roleArchive = bind("DELETE", "/admin/roles/{id}",
                                      [roles, origin](const INFRA::HttpRequest &request)
                                      {
                                          return originAllowed(origin, request) ?
                                              withCors(roleArchiveResponse(*roles, request), request) :
                                              RestApi::error(ErrorCode::Forbidden, request.requestId);
                                      });
        if (!roleArchive.ok)
            return roleArchive;
        const auto rolePermissionsGet = bind("GET", "/admin/roles/{id}/permissions",
                                             [roles, origin](const INFRA::HttpRequest &request)
                                             {
                                                 return originAllowed(origin, request) ?
                                                     withCors(roleCollectionGet(*roles, request, false), request) :
                                                     RestApi::error(ErrorCode::Forbidden, request.requestId);
                                             });
        if (!rolePermissionsGet.ok)
            return rolePermissionsGet;
        const auto rolePermissionsPut = bind("PUT", "/admin/roles/{id}/permissions",
                                             [roles, origin](const INFRA::HttpRequest &request)
                                             {
                                                 return originAllowed(origin, request) ?
                                                     withCors(roleCollectionPut(*roles, request, false), request) :
                                                     RestApi::error(ErrorCode::Forbidden, request.requestId);
                                             });
        if (!rolePermissionsPut.ok)
            return rolePermissionsPut;
        const auto roleScopesGet = bind("GET", "/admin/roles/{id}/manage-scopes",
                                        [roles, origin](const INFRA::HttpRequest &request)
                                        {
                                            return originAllowed(origin, request) ?
                                                withCors(roleCollectionGet(*roles, request, true), request) :
                                                RestApi::error(ErrorCode::Forbidden, request.requestId);
                                        });
        if (!roleScopesGet.ok)
            return roleScopesGet;
        const auto roleScopesPut = bind("PUT", "/admin/roles/{id}/manage-scopes",
                                        [roles, origin](const INFRA::HttpRequest &request)
                                        {
                                            return originAllowed(origin, request) ?
                                                withCors(roleCollectionPut(*roles, request, true), request) :
                                                RestApi::error(ErrorCode::Forbidden, request.requestId);
                                        });
        if (!roleScopesPut.ok)
            return roleScopesPut;

        const auto permissionList = bind("GET", "/admin/permissions",
                                         [permissions, origin](const INFRA::HttpRequest &request)
                                         {
                                             return originAllowed(origin, request) ?
                                                 withCors(permissionListResponse(*permissions, request), request) :
                                                 RestApi::error(ErrorCode::Forbidden, request.requestId);
                                         });
        if (!permissionList.ok)
            return permissionList;
        const auto permissionGet = bind("GET", "/admin/permissions/{id}",
                                        [permissions, origin](const INFRA::HttpRequest &request)
                                        {
                                            return originAllowed(origin, request) ?
                                                withCors(permissionGetResponse(*permissions, request), request) :
                                                RestApi::error(ErrorCode::Forbidden, request.requestId);
                                        });
        if (!permissionGet.ok)
            return permissionGet;
        const auto permissionCreate = bind("POST", "/admin/permissions",
                                           [permissions, origin](const INFRA::HttpRequest &request)
                                           {
                                               return originAllowed(origin, request) ?
                                                   withCors(permissionCreateResponse(*permissions, request), request) :
                                                   RestApi::error(ErrorCode::Forbidden, request.requestId);
                                           });
        if (!permissionCreate.ok)
            return permissionCreate;
        const auto permissionUpdate = bind("PATCH", "/admin/permissions/{id}",
                                           [permissions, origin](const INFRA::HttpRequest &request)
                                           {
                                               return originAllowed(origin, request) ?
                                                   withCors(permissionUpdateResponse(*permissions, request), request) :
                                                   RestApi::error(ErrorCode::Forbidden, request.requestId);
                                           });
        if (!permissionUpdate.ok)
            return permissionUpdate;
        const auto permissionStatus = bind("PATCH", "/admin/permissions/{id}/status",
                                           [permissions, origin](const INFRA::HttpRequest &request)
                                           {
                                               return originAllowed(origin, request) ?
                                                   withCors(permissionStatusResponse(*permissions, request), request) :
                                                   RestApi::error(ErrorCode::Forbidden, request.requestId);
                                           });
        if (!permissionStatus.ok)
            return permissionStatus;
        const auto permissionArchive = bind("DELETE", "/admin/permissions/{id}",
                                            [permissions, origin](const INFRA::HttpRequest &request)
                                            {
                                                return originAllowed(origin, request) ?
                                                    withCors(permissionArchiveResponse(*permissions, request), request) :
                                                    RestApi::error(ErrorCode::Forbidden, request.requestId);
                                            });
        if (!permissionArchive.ok)
            return permissionArchive;

        const auto preflight = preflightHandler(origin, "GET, POST, PATCH, PUT, DELETE, OPTIONS");
        for (const auto path : {"/admin/roles", "/admin/roles/{id}", "/admin/roles/{id}/status", "/admin/roles/{id}/permissions",
                                "/admin/roles/{id}/manage-scopes", "/admin/permissions", "/admin/permissions/{id}",
                                "/admin/permissions/{id}/status"})
        {
            const auto options = bind("OPTIONS", path, preflight);
            if (!options.ok)
                return options;
        }
        return SZX::COMMON::Result<void>::Success();
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN::RBAC
