#include "SzxAdmin/Infra/HttpTransport.h"

#include "Serializer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/RequestContext.h"
#include "hv/HttpServer.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <utility>

namespace SZX::ADMIN::INFRA
{
namespace
{
auto errorResponse(ErrorCode code, std::string_view requestId, std::string_view message = {}) -> HttpResponse
{
    const auto effectiveMessage = message.empty() ? defaultErrorMessage(code) : message;
    HttpResponse response{.statusCode = errorCodeHttpStatus(code)};
    response.headers.emplace("Content-Type", "application/json");
    response.headers.emplace(std::string{kRequestIdHeader}, std::string{requestId});
    const SZX::COMMON::Json payload{{"code", std::string{errorCodeName(code)}},
                                    {"message", std::string{effectiveMessage}},
                                    {"details", SZX::COMMON::Json::object()},
                                    {"request_id", std::string{requestId}}};
    response.body = payload.dump();
    return response;
}

auto validMethod(std::string_view method) noexcept -> bool
{
    return method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE";
}

auto validPath(std::string_view path) noexcept -> bool
{
    return !path.empty() && path.front() == '/' && path.find('?') == std::string_view::npos &&
        path.find('#') == std::string_view::npos;
}

auto convertRequest(::HttpRequest *request) -> HttpRequest
{
    HttpRequest converted{.method = request->Method(), .path = request->Path(), .body = request->body};
    converted.requestId = resolveRequestId(request->GetHeader(std::string{kRequestIdHeader}.c_str()));
    for (const auto &[name, value] : request->headers)
        converted.headers.emplace(name, value);
    return converted;
}

auto applyResponse(const HttpResponse &source, ::HttpResponse *target, const std::string &requestId) -> int
{
    auto response = source;
    if (response.statusCode < 100 || response.statusCode > 599)
        response = errorResponse(ErrorCode::InternalFailure, requestId);
    response.headers[std::string{kRequestIdHeader}] = requestId;
    if (!response.headers.contains("Content-Type"))
        response.headers.emplace("Content-Type", "application/json");
    target->status_code = static_cast<http_status>(response.statusCode);
    target->body        = std::move(response.body);
    for (const auto &[name, value] : response.headers)
        target->SetHeader(name.c_str(), value);
    return target->status_code;
}
} // namespace

class HttpTransport::Impl final
{
public:
    explicit Impl(ServerConfig serverConfig) :
        config_(std::move(serverConfig)),
        server_(&router_)
    {
        router_.GET(healthPath().c_str(),
                    [this](::HttpRequest *request, ::HttpResponse *response)
                    {
                        const auto requestId = resolveRequestId(request->GetHeader(std::string{kRequestIdHeader}.c_str()));
                        return applyResponse(HttpTransport::healthResponse(requestId), response, requestId);
                    });
        router_.Use(
            [this](::HttpRequest *request, ::HttpResponse *response)
            {
                const auto requestId = resolveRequestId(request->GetHeader(std::string{kRequestIdHeader}.c_str()));
                request->SetHeader(std::string{kRequestIdHeader}.c_str(), requestId);
                response->SetHeader(std::string{kRequestIdHeader}.c_str(), requestId);
                if (request->ContentLength() > config_.maxRequestBodyBytes || request->body.size() > config_.maxRequestBodyBytes)
                {
                    return applyResponse(errorResponse(ErrorCode::PayloadTooLarge, requestId), response, requestId);
                }
                return HTTP_STATUS_NEXT;
            });
        router_.enable_access_log = config_.accessLog;
        router_.errorHandler      = [](::HttpRequest *request, ::HttpResponse *response)
        {
            const auto requestId = resolveRequestId(request->GetHeader(std::string{kRequestIdHeader}.c_str()));
            const auto code      = response->status_code == HTTP_STATUS_METHOD_NOT_ALLOWED ? ErrorCode::MethodNotAllowed :
                                                                                             ErrorCode::ResourceNotFound;
            return applyResponse(errorResponse(code, requestId), response, requestId);
        };
    }

    [[nodiscard]] auto healthPath() const -> std::string
    {
        return config_.apiPrefix == "/" ? "/health" : config_.apiPrefix + "/health";
    }

    [[nodiscard]] auto registerRoute(std::string method, std::string path, HttpHandler handler) -> SZX::COMMON::Result<void>
    {
        try
        {
            std::lock_guard lock(mutex_);
            if (started_)
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::TransportFailure), "HTTP 服务已启动。");
            if (!validMethod(method) || !validPath(path) || !handler)
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::ValidationError), "HTTP 路由参数无效。");
            if (path == healthPath())
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::RouteConflict), "健康检查路由不可覆盖。");
            const auto route = std::move(path);
            router_.Handle(
                method.c_str(), route.c_str(),
                [handler = std::move(handler), maxBodyBytes = config_.maxRequestBodyBytes](
                    const ::HttpContextPtr &context, http_parser_state state, const char *data, std::size_t size)
                {
                    auto *request  = context->request.get();
                    auto *response = context->response.get();
                    if (state == HP_HEADERS_COMPLETE)
                    {
                        if (request->ContentLength() > maxBodyBytes)
                            response->status_code = HTTP_STATUS_PAYLOAD_TOO_LARGE;
                        return HTTP_STATUS_NEXT;
                    }
                    if (state == HP_BODY)
                    {
                        if (response->status_code == HTTP_STATUS_PAYLOAD_TOO_LARGE)
                            return HTTP_STATUS_NEXT;
                        if (size > maxBodyBytes || request->body.size() > maxBodyBytes - size)
                        {
                            request->body.clear();
                            response->status_code = HTTP_STATUS_PAYLOAD_TOO_LARGE;
                            return HTTP_STATUS_NEXT;
                        }
                        request->body.append(data, size);
                        return HTTP_STATUS_NEXT;
                    }
                    if (state != HP_MESSAGE_COMPLETE)
                        return HTTP_STATUS_NEXT;

                    const auto requestId = resolveRequestId(request->GetHeader(std::string{kRequestIdHeader}.c_str()));
                    try
                    {
                        if (response->status_code == HTTP_STATUS_PAYLOAD_TOO_LARGE)
                            return applyResponse(errorResponse(ErrorCode::PayloadTooLarge, requestId), response, requestId);
                        const auto converted = convertRequest(request);
                        return applyResponse(handler(converted), response, converted.requestId);
                    } catch (const std::exception &)
                    {
                        return applyResponse(errorResponse(ErrorCode::InternalFailure, requestId), response, requestId);
                    } catch (...)
                    {
                        return applyResponse(errorResponse(ErrorCode::InternalFailure, requestId), response, requestId);
                    }
                });
            return SZX::COMMON::Result<void>::Success();
        } catch (const std::exception &)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 路由注册失败。");
        } catch (...)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 路由注册失败。");
        }
    }

    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>
    {
        try
        {
            std::lock_guard lock(mutex_);
            if (started_)
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::TransportFailure), "HTTP 服务已启动。");
            const auto valid = config_.validate();
            if (!valid.ok)
                return valid;
            server_.setHost(config_.host.c_str());
            server_.setPort(config_.port);
            server_.setThreadNum(config_.workerThreads);
            server_.setMaxWorkerConnectionNum(config_.maxConnections);
            if (server_.start() != 0)
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::TransportFailure), "HTTP 服务启动失败。");
            started_ = true;
            return SZX::COMMON::Result<void>::Success();
        } catch (const std::exception &)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 服务启动失败。");
        } catch (...)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 服务启动失败。");
        }
    }

    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>
    {
        try
        {
            std::lock_guard lock(mutex_);
            if (!started_)
                return SZX::COMMON::Result<void>::Success();
            if (server_.stop() != 0)
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::TransportFailure), "HTTP 服务停止失败。");
            started_ = false;
            return SZX::COMMON::Result<void>::Success();
        } catch (const std::exception &)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 服务停止失败。");
        } catch (...)
        {
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::InternalFailure), "HTTP 服务停止失败。");
        }
    }

    ServerConfig config_;
    hv::HttpService router_;
    hv::HttpServer server_;
    mutable std::mutex mutex_;
    std::atomic_bool started_{};
};

HttpTransport::HttpTransport(ServerConfig config) :
    impl_(std::make_unique<Impl>(std::move(config)))
{
}

HttpTransport::~HttpTransport() = default;

auto HttpTransport::registerRoute(std::string method, std::string path, HttpHandler handler) -> SZX::COMMON::Result<void>
{
    return impl_->registerRoute(std::move(method), std::move(path), std::move(handler));
}

auto HttpTransport::start() -> SZX::COMMON::Result<void>
{
    return impl_->start();
}

auto HttpTransport::stop() -> SZX::COMMON::Result<void>
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
    HttpResponse response;
    response.headers.emplace("Content-Type", "application/json");
    response.headers.emplace(std::string{kRequestIdHeader}, requestId);
    const SZX::COMMON::Json payload{{"data", SZX::COMMON::Json{{"status", "ok"}}}, {"request_id", requestId}};
    response.body = payload.dump();
    return response;
}
} // namespace SZX::ADMIN::INFRA
