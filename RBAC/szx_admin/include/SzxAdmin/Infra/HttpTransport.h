#pragma once

#include "Result.hpp"
#include "SzxAdmin/Infra/ServerConfig.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace SZX::ADMIN::INFRA
{
struct HttpRequest final
{
    std::string method;
    std::string path;
    std::string body;
    std::string requestId;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> pathParams;

    /** Case-insensitive lookup; the returned view is valid while headers remain unchanged. */
    [[nodiscard]] auto header(std::string_view name) const noexcept -> std::optional<std::string_view>;
};

struct HttpResponse final
{
    int statusCode{200};
    std::string body;
    std::map<std::string, std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest &)>;

class HttpTransport final
{
public:
    explicit HttpTransport(ServerConfig config);
    ~HttpTransport();

    HttpTransport(const HttpTransport &)            = delete;
    HttpTransport &operator=(const HttpTransport &) = delete;
    HttpTransport(HttpTransport &&)                 = delete;
    HttpTransport &operator=(HttpTransport &&)      = delete;

    [[nodiscard]] auto registerRoute(std::string method, std::string path, HttpHandler handler) -> SZX::COMMON::Result<void>;
    /** Register an API-relative path (e.g. /admin/users/{id}) below ServerConfig::apiPrefix. */
    [[nodiscard]] auto registerApiRoute(std::string method, std::string path, HttpHandler handler) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;

    [[nodiscard]] auto running() const noexcept -> bool;
    [[nodiscard]] auto port() const noexcept -> std::uint16_t;
    [[nodiscard]] auto config() const noexcept -> const ServerConfig &;

    [[nodiscard]] static auto healthResponse(const std::string &requestId) -> HttpResponse;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace SZX::ADMIN::INFRA
