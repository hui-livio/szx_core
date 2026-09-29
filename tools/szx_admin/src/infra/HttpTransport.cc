#include "SzxAdmin/Infra/HttpTransport.h"

#include "../Routes.h"

#include "Defer.hpp"
#include "JsonHelper.hpp"
#include "Logger.h"
#include "Serializer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/RequestContext.h"
#include "SzxAdmin/Infra/RestApi.h"
#include "SzxNetwork/HttpServer.h"
#include "TimeHelper.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <utility>

namespace SZX::ADMIN::INFRA
{
namespace
{
namespace Network = SZX::NETWORK;
using SZX::COMMON::Result;
thread_local bool inTransportCallback         = false;
constexpr std::string_view requestIdAttribute = "szx.admin.request_id";

auto failure(ErrorCode code) -> Result<void>
{
    return Result<void>::Failure(static_cast<int>(code), {});
}

auto networkResult(const Result<void> &result, ErrorCode invalid = ErrorCode::ValidationError) -> Result<void>
{
    if (result.ok)
        return Result<void>::Success();
    switch (static_cast<Network::NetworkError>(result.error))
    {
    case Network::NetworkError::InvalidArgument:
        return failure(invalid);
    case Network::NetworkError::RouteConflict:
        return failure(ErrorCode::RouteConflict);
    case Network::NetworkError::Internal:
        return failure(ErrorCode::InternalFailure);
    default:
        return failure(ErrorCode::TransportFailure);
    }
}

auto responseError(int status) noexcept -> ErrorCode
{
    switch (status)
    {
    case 400:
    case 417:
    case 431:
        return ErrorCode::ValidationError;
    case 403:
        return ErrorCode::Forbidden;
    case 404:
        return ErrorCode::ResourceNotFound;
    case 405:
        return ErrorCode::MethodNotAllowed;
    case 413:
        return ErrorCode::PayloadTooLarge;
    default:
        return ErrorCode::InternalFailure;
    }
}

auto requestId(const Network::HttpServerRequest &request) -> std::string
{
    const auto id = request.attributes.find(std::string{requestIdAttribute});
    if (id != request.attributes.end())
        return id->second;
    const auto header = request.headers.find(kRequestIdHeader);
    return resolveRequestId(header == request.headers.end() ? std::string_view{} : header->second);
}

auto convertRequest(const Network::HttpServerRequest &request) -> HttpRequest
{
    HttpRequest converted{.method = request.method, .path = request.path, .body = request.body, .requestId = requestId(request)};
    converted.headers.insert(request.headers.begin(), request.headers.end());
    const auto originalId = request.headers.find(kRequestIdHeader);
    if (originalId != request.headers.end())
        converted.headers.erase(originalId->first);
    converted.headers[std::string{kRequestIdHeader}] = converted.requestId;
    converted.query                                  = request.query;
    converted.pathParams                             = request.pathParams;
    converted.peerAddress                            = request.peer.host;
    return converted;
}

void secureResponse(HttpResponse &response)
{
    // Apply at the transport boundary, including static pages and protocol errors.
    response.headers["Content-Security-Policy"] =
        "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; font-src 'self' data:; connect-src 'self'; "
        "object-src 'none'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'";
    response.headers["X-Content-Type-Options"] = "nosniff";
    response.headers["X-Frame-Options"]        = "DENY";
    response.headers["Referrer-Policy"]        = "no-referrer";
    response.headers["Permissions-Policy"]     = "camera=(), microphone=(), geolocation=()";
}

auto prepareResponse(HttpResponse response, const std::string &id) -> HttpResponse
{
    if (response.statusCode < 200 || response.statusCode > 599)
        response = RestApi::error(ErrorCode::InternalFailure, id);
    response.headers[std::string{kRequestIdHeader}] = id;
    if (!response.headers.contains("Content-Type"))
        response.headers.emplace("Content-Type", "application/json");
    secureResponse(response);
    return response;
}

auto validMethod(std::string_view method) noexcept -> bool
{
    return method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" || method == "PATCH" ||
        method == "DELETE" || method == "OPTIONS";
}

void normalizePath(std::string &path)
{
    // Collapse only literal separators. Percent-encoded data remains untouched.
    path.erase(std::unique(path.begin(), path.end(), [](char left, char right) { return left == '/' && right == '/'; }),
               path.end());
}

auto isApiPath(std::string_view path, std::string_view prefix) noexcept -> bool
{
    return prefix == "/" || path == prefix ||
        (path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/');
}
} // namespace

class HttpTransport::Impl final
{
public:
    explicit Impl(ServerConfig config) :
        config_(std::move(config))
    {
    }

    auto healthPath() const -> std::string
    {
        return config_.apiPrefix == "/" ? "/health" : config_.apiPrefix + "/health";
    }

    auto isHealthPath(std::string_view path) const -> bool
    {
        if (path.ends_with('/'))
            path.remove_suffix(1);
        return path == healthPath();
    }

    void accessLog(const Network::HttpServerRequest &request, const std::string &id, const Network::HttpServerResponse &response,
                   std::string_view requestStartedAt, std::chrono::steady_clock::time_point started) const noexcept
    {
        if (!config_.accessLog)
            return;
        try
        {
            const auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
            const auto path =
                SZX::COMMON::Json(request.path.substr(0, 512)).dump(-1, ' ', false, SZX::COMMON::Json::error_handler_t::replace);
            if (response.statusCode >= 400)
            {
                const auto body = SZX::COMMON::JsonHelper::ParseRequiredObject(response.body);
                if (body && body->contains("code") && body->at("code").is_number_integer())
                {
                    LOGM_INFO("szx_admin.http",
                              "request_id={} request_started_at={} duration_microseconds={} method={} path={} status={} code={}",
                              id, requestStartedAt, duration, request.method, path, response.statusCode,
                              body->at("code").get<int>());
                    return;
                }
            }
            LOGM_INFO("szx_admin.http",
                      "request_id={} request_started_at={} duration_microseconds={} method={} path={} status={}", id,
                      requestStartedAt, duration, request.method, path, response.statusCode);
        } catch (...)
        {
        }
    }

    auto notFound(const Network::HttpServerRequest &request) const -> Network::HttpServerResponse
    {
        if (request.method != "GET" && request.method != "HEAD")
            return {.statusCode = 404};
        if (isApiPath(request.path, config_.apiPrefix))
        {
            if (!apiDocsHandler_)
                return {.statusCode = 404};
            if (request.path == config_.apiPrefix && config_.apiPrefix != "/")
                return {.statusCode = 308, .headers = {{"Location", config_.apiPrefix + "/"}}};
            auto mounted = request;
            mounted.path = config_.apiPrefix == "/" ? request.path : request.path.substr(config_.apiPrefix.size());
            // Only documentation paths are static. Unknown API routes never receive SPA HTML.
            if (mounted.path != "/" && mounted.path != "/openapi.json" && !mounted.path.starts_with("/docs/"))
                return {.statusCode = 404};
            if (mounted.path.starts_with("/docs/"))
                mounted.path.erase(0, 5);
            return apiDocsHandler_(mounted);
        }
        if (!staticHandler_)
            return {.statusCode = 404};
        if (isApiPath(request.path, "/admin"))
        {
            if (!adminStaticHandler_)
                return {.statusCode = 404};
            if (request.path == "/admin")
                return {.statusCode = 308, .headers = {{"Location", "/admin/"}}};
            auto mounted  = request;
            mounted.path  = request.path.substr(6);
            auto response = adminStaticHandler_(mounted);
            if (response.statusCode == 404 && config_.spaFallback && !std::filesystem::path{mounted.path}.has_extension())
            {
                mounted.path = "/";
                response     = adminStaticHandler_(mounted);
            }
            return response;
        }
        auto response = staticHandler_(request);
        // Failed confinement never reaches the SPA fallback. Missing assets retain their 404.
        if (response.statusCode == 404 && config_.spaFallback && !std::filesystem::path{request.path}.has_extension())
        {
            auto indexRequest = request;
            indexRequest.path = "/";
            response          = staticHandler_(indexRequest);
        }
        return response;
    }

    auto ensureServer() -> Result<void>
    {
        if (server_)
            return Result<void>::Success();
        const auto valid = config_.validate();
        if (!valid.ok)
            return valid;
        Network::HttpServerOptions options;
        options.listen        = {config_.host, config_.port};
        options.ioThreadCount = std::max<std::size_t>(1, config_.workerThreads);
        options.maxBodyBytes  = config_.maxRequestBodyBytes;
        auto created          = Network::HttpServer::Create(options);
        if (!created.ok)
            return networkResult(Result<void>::Failure(created.error, {}), ErrorCode::InvalidConfiguration);
        auto server     = std::move(created.value);
        auto configured = server->setMaxConnections(config_.maxConnections);
        if (!configured.ok)
            return networkResult(configured, ErrorCode::InvalidConfiguration);
        configured = server->useBeforeRouting(
            [](auto &request, const auto &next)
            {
                normalizePath(request.path);
                return next(request);
            });
        if (!configured.ok)
            return networkResult(configured);
        configured = server->use(
            [this](auto &request, const auto &next)
            {
                const auto previous = inTransportCallback;
                inTransportCallback = true;
                defer
                {
                    inTransportCallback = previous;
                };
                const auto requestStartedAt                         = SZX::COMMON::TimeHelper::NowIso8601();
                const auto started                                  = std::chrono::steady_clock::now();
                const auto id                                       = requestId(request);
                request.attributes[std::string{requestIdAttribute}] = id;
                request.headers[std::string{kRequestIdHeader}]      = id;
                auto response                                       = next(request);
                if (response.body.empty() && response.statusCode >= 400)
                {
                    auto error       = prepareResponse(RestApi::error(responseError(response.statusCode), id), id);
                    error.statusCode = response.statusCode;
                    response         = std::move(error);
                }
                response.headers[std::string{kRequestIdHeader}] = id;
                secureResponse(response);
                accessLog(request, id, response, requestStartedAt, started);
                return response;
            });
        if (!configured.ok)
            return networkResult(configured);
        configured = server->onRequestError(
            [this](const auto &request, int status, auto)
            {
                const auto previous = inTransportCallback;
                inTransportCallback = true;
                defer
                {
                    inTransportCallback = previous;
                };
                const auto requestStartedAt = SZX::COMMON::TimeHelper::NowIso8601();
                const auto started          = std::chrono::steady_clock::now();
                const auto id               = requestId(request);
                auto response               = prepareResponse(RestApi::error(responseError(status), id), id);
                accessLog(request, id, response, requestStartedAt, started);
                return response;
            });
        if (!configured.ok)
            return networkResult(configured);
        configured = server->onNotFound([this](const auto &request) { return notFound(request); });
        if (!configured.ok)
            return networkResult(configured);
        const auto healthRoutes = registerHealthRoutes(
            config_.apiPrefix,
            [&](std::string path, HttpHandler handler)
            {
                return networkResult(server->get(std::move(path),
                                                 [handler = std::move(handler)](const auto &request)
                                                 {
                                                     const auto converted = convertRequest(request);
                                                     return prepareResponse(handler(converted), converted.requestId);
                                                 }),
                                     ErrorCode::InvalidConfiguration);
            });
        if (!healthRoutes.ok)
            return healthRoutes;
        server_ = std::move(server);
        return Result<void>::Success();
    }

    auto registerRoute(std::string method, std::string path, HttpHandler handler) -> Result<void>
    {
        if (inTransportCallback)
            return failure(ErrorCode::TransportFailure);
        try
        {
            std::lock_guard lock(mutex_);
            if (started_)
                return failure(ErrorCode::TransportFailure);
            if (!validMethod(method) || !handler)
                return failure(ErrorCode::ValidationError);
            normalizePath(path);
            if (isHealthPath(path))
                return failure(ErrorCode::RouteConflict);
            const auto ready = ensureServer();
            if (!ready.ok)
                return ready;
            const auto registeredMethod = method;
            const auto registeredPath   = path;
            const auto result            = networkResult(server_->route(std::move(method), std::move(path),
                                                [handler = std::move(handler)](const auto &request)
                                                {
                                                    const auto converted = convertRequest(request);
                                                    return prepareResponse(handler(converted), converted.requestId);
                                                }));
            if (result.ok)
                LOGM_DEBUG("szx_admin.http", "event=route_registered method={} path={}", registeredMethod, registeredPath);
            return result;
        } catch (...)
        {
            return failure(ErrorCode::InternalFailure);
        }
    }

    auto start() -> Result<void>
    {
        if (inTransportCallback)
            return failure(ErrorCode::TransportFailure);
        try
        {
            std::lock_guard lock(mutex_);
            if (started_)
                return failure(ErrorCode::TransportFailure);
            LOGM_INFO("szx_admin.http", "event=server_start_begin host={} port={} workers={} max_connections={}", config_.host,
                      config_.port, (std::max<std::size_t>)(1, config_.workerThreads), config_.maxConnections);
            const auto ready = ensureServer();
            if (!ready.ok)
                return ready;
            staticHandler_      = {};
            adminStaticHandler_ = {};
            apiDocsHandler_     = {};
            if (!config_.staticRoot.empty())
            {
                std::error_code error;
                const bool exists = std::filesystem::exists(config_.staticRoot, error);
                if (error)
                    return failure(ErrorCode::InvalidConfiguration);
                if (exists)
                {
                    LOGM_INFO("szx_admin.http", "event=static_root_mount path={}", config_.staticRoot);
                    auto files = Network::HttpServer::CreateStaticHandler(config_.staticRoot, config_.staticHomePage);
                    if (!files.ok)
                        return networkResult(Result<void>::Failure(files.error, {}), ErrorCode::InvalidConfiguration);
                    staticHandler_       = std::move(files.value);
                    const auto docsRoot  = std::filesystem::path{config_.staticRoot} / "api-docs";
                    const bool docsExist = std::filesystem::exists(docsRoot, error);
                    if (error)
                        return failure(ErrorCode::InvalidConfiguration);
                    if (docsExist)
                    {
                        auto docs = Network::HttpServer::CreateStaticHandler(docsRoot.string(), "index.html");
                        if (!docs.ok)
                            return networkResult(Result<void>::Failure(docs.error, {}), ErrorCode::InvalidConfiguration);
                        apiDocsHandler_ = std::move(docs.value);
                        LOGM_INFO("szx_admin.http", "event=api_docs_mount path={}", docsRoot.string());
                    }
                    else
                        LOGM_DEBUG("szx_admin.http", "event=api_docs_not_mounted path={} reason=not_found", docsRoot.string());
                    const auto adminRoot   = std::filesystem::path{config_.staticRoot} / "admin";
                    const bool adminExists = std::filesystem::exists(adminRoot, error);
                    if (error)
                        return failure(ErrorCode::InvalidConfiguration);
                    if (adminExists)
                    {
                        auto adminFiles = Network::HttpServer::CreateStaticHandler(adminRoot.string(), "index.html");
                        if (!adminFiles.ok)
                            return networkResult(Result<void>::Failure(adminFiles.error, {}), ErrorCode::InvalidConfiguration);
                        adminStaticHandler_ = std::move(adminFiles.value);
                        LOGM_INFO("szx_admin.http", "event=admin_static_mount path={}", adminRoot.string());
                    }
                    else
                        LOGM_DEBUG("szx_admin.http", "event=admin_static_not_mounted path={} reason=not_found",
                                   adminRoot.string());
                }
                else
                    LOGM_DEBUG("szx_admin.http", "event=static_root_not_mounted path={} reason=not_found", config_.staticRoot);
            }
            const auto result = server_->start();
            if (!result.ok)
                return networkResult(result, ErrorCode::InvalidConfiguration);
            started_ = true;
            LOGM_INFO("szx_admin.http", "event=server_start_complete host={} port={}", config_.host, config_.port);
            return Result<void>::Success();
        } catch (...)
        {
            return failure(ErrorCode::InternalFailure);
        }
    }

    auto stop() -> Result<void>
    {
        // Reject before locking: another thread may be holding the lifecycle lock while joining us.
        if (inTransportCallback)
            return failure(ErrorCode::TransportFailure);
        try
        {
            std::lock_guard lock(mutex_);
            if (!started_)
                return Result<void>::Success();
            const auto result = server_->stop();
            if (!result.ok)
                return networkResult(result);
            started_ = false;
            LOGM_INFO("szx_admin.http", "event=server_stop_complete host={} port={}", config_.host, config_.port);
            return Result<void>::Success();
        } catch (...)
        {
            return failure(ErrorCode::InternalFailure);
        }
    }

    ServerConfig config_;
    std::mutex mutex_;
    std::atomic_bool started_{};
    Network::HttpRouteHandler staticHandler_;
    Network::HttpRouteHandler adminStaticHandler_;
    Network::HttpRouteHandler apiDocsHandler_;
    std::unique_ptr<Network::HttpServer> server_; // Join before releasing captured configuration and file handler.
};

HttpTransport::HttpTransport(ServerConfig config) :
    impl_(std::make_unique<Impl>(std::move(config)))
{
}
HttpTransport::~HttpTransport() = default;

auto HttpRequest::header(std::string_view name) const noexcept -> std::optional<std::string_view>
{
    const Network::CaseInsensitiveLess less;
    std::optional<std::string_view> found;
    for (const auto &[key, value] : headers)
    {
        if (!less(key, name) && !less(name, key))
        {
            if (found)
                return std::nullopt;
            found = value;
        }
    }
    return found;
}

auto HttpTransport::registerRoute(std::string method, std::string path, HttpHandler handler) -> Result<void>
{
    return impl_->registerRoute(std::move(method), std::move(path), std::move(handler));
}
auto HttpTransport::registerApiRoute(std::string method, std::string path, HttpHandler handler) -> Result<void>
{
    try
    {
        if (path.empty() || path.front() != '/' || path.find_first_of("?#") != std::string::npos)
            return failure(ErrorCode::ValidationError);
        const auto &prefix = impl_->config_.apiPrefix;
        return impl_->registerRoute(std::move(method), prefix == "/" ? std::move(path) : prefix + path, std::move(handler));
    } catch (...)
    {
        return failure(ErrorCode::InternalFailure);
    }
}
auto HttpTransport::start() -> Result<void>
{
    return impl_->start();
}
auto HttpTransport::stop() -> Result<void>
{
    return impl_->stop();
}
auto HttpTransport::running() const noexcept -> bool
{
    return impl_->started_;
}
auto HttpTransport::port() const noexcept -> std::uint16_t
{
    return impl_->config_.port;
}
auto HttpTransport::config() const noexcept -> const ServerConfig &
{
    return impl_->config_;
}

auto HttpTransport::healthResponse(const std::string &requestId) -> HttpResponse
{
    return RestApi::success(SZX::COMMON::Json{{"status", "ok"}}, requestId);
}
} // namespace SZX::ADMIN::INFRA
