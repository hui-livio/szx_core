#include "SzxNetwork/HttpServer.h"

#include "Defer.hpp"
#include "FileHelper.hpp"
#include "detail/HttpTransport.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace SZX::NETWORK::v1
{
namespace
{
using SZX::COMMON::Result;
thread_local bool inNetworkCallback  = false;
constexpr std::size_t maxStaticBytes = 16u * 1024u * 1024u;

template <typename T = void>
auto failure(NetworkError error) -> Result<T>
{
    // Empty diagnostics also keep the allocation-failure fallback allocation-free.
    return Result<T>::Failure(static_cast<int>(error), {});
}

template <typename Fn>
auto boundary(Fn &&fn) -> decltype(fn())
{
    try
    {
        return fn();
    } catch (...)
    {
        return decltype(fn())::Failure(static_cast<int>(NetworkError::Internal), {});
    }
}

auto segments(std::string_view path) -> std::vector<std::string>
{
    std::vector<std::string> result;
    std::size_t begin = 1;
    while (begin <= path.size())
    {
        const auto end = path.find('/', begin);
        result.emplace_back(path.substr(begin, end == path.npos ? path.size() - begin : end - begin));
        if (end == path.npos)
            break;
        begin = end + 1;
    }
    return result;
}

auto decodePath(std::string_view path) -> std::optional<std::string>
{
    const auto hex = [](unsigned char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    std::string result;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        auto c = static_cast<unsigned char>(path[i]);
        if (c == '%')
        {
            if (i + 2 >= path.size())
                return std::nullopt;
            const int a = hex(path[i + 1]);
            const int b = hex(path[i + 2]);
            if (a < 0 || b < 0)
                return std::nullopt;
            c  = static_cast<unsigned char>(a * 16 + b);
            i += 2;
        }
        if (c < 32 || c == 127)
            return std::nullopt;
        result.push_back(static_cast<char>(c));
    }
    return result;
}

auto validPath(std::string_view path) -> bool
{
    if (path.empty() || path.front() != '/')
        return false;
    return std::none_of(path.begin(), path.end(), [](unsigned char c)
                        { return c <= 32 || c == 127 || c == '?' || c == '#' || c == '\\' || c == ':' || c == '%'; });
}

auto validMethod(std::string_view method) -> bool
{
    return method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" || method == "DELETE" ||
        method == "PATCH" || method == "OPTIONS" || method == "TRACE";
}

auto routeShape(const std::string &path) -> std::optional<std::string>
{
    if (!validPath(path))
        return std::nullopt;
    const auto parts = segments(path);
    std::set<std::string> names;
    std::string shape;
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        const auto &part  = parts[i];
        shape            += '/';
        if (part == "*")
        {
            if (i + 1 != parts.size())
                return std::nullopt;
            shape += '*';
        }
        else if (part.size() > 2 && part.front() == '{' && part.back() == '}')
        {
            auto name = part.substr(1, part.size() - 2);
            if (!std::all_of(
                    name.begin(), name.end(), [](unsigned char c)
                    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }) ||
                !names.insert(name).second)
                return std::nullopt;
            shape += "{}";
        }
        else
        {
            if (part == "." || part == ".." || part.find_first_of("{}*") != part.npos)
                return std::nullopt;
            shape += part;
        }
    }
    return shape;
}

auto underPrefix(std::string_view path, std::string_view prefix) -> bool
{
    return path.starts_with('/') &&
        (prefix == "/" || path == prefix ||
         (path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '/'));
}

// Filesystem confinement is domain-specific; common's path helpers do not guarantee it.
auto confinedPath(const std::filesystem::path &root, const std::string &relative) -> std::optional<std::filesystem::path>
{
    if (relative.find_first_of("\\:%") != relative.npos || relative.find('\0') != relative.npos)
        return std::nullopt;
    const std::filesystem::path path{relative};
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return std::nullopt;
    for (const auto &part : path)
    {
        const auto text = part.string();
        // Trailing spaces/dots and DOS device names can alias other paths on Windows.
        if (text == ".." || text == "." || (!text.empty() && (text.back() == '.' || text.back() == ' ')))
            return std::nullopt;
#ifdef _WIN32
        auto base = part.stem().string();
        std::transform(base.begin(), base.end(), base.begin(),
                       [](unsigned char c) { return static_cast<char>(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c); });
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
            (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) && base[3] >= '1' && base[3] <= '9'))
            return std::nullopt;
#endif
    }
    std::error_code error;
    auto candidate = std::filesystem::weakly_canonical(root / path, error);
    if (error)
        return std::nullopt;
    auto r = root.begin();
    auto c = candidate.begin();
    for (; r != root.end(); ++r, ++c)
        if (c == candidate.end() || *r != *c)
            return std::nullopt;
    return candidate;
}

auto contentType(const std::filesystem::path &path) -> std::string
{
    const auto ext = SZX::COMMON::FileHelper::Extension(path);
    if (ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";
    if (ext == ".css")
        return "text/css; charset=utf-8";
    if (ext == ".js" || ext == ".mjs")
        return "text/javascript; charset=utf-8";
    if (ext == ".json")
        return "application/json";
    if (ext == ".svg")
        return "image/svg+xml";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".ico")
        return "image/x-icon";
    if (ext == ".txt")
        return "text/plain; charset=utf-8";
    if (ext == ".wasm")
        return "application/wasm";
    return "application/octet-stream";
}

auto validResponse(const HttpServerResponse &response) -> bool
{
    if (response.statusCode < 200 || response.statusCode > 599)
        return false;
    for (const auto &[name, value] : response.headers)
    {
        if (name.empty() ||
            std::any_of(name.begin(), name.end(),
                        [](unsigned char c)
                        {
                            return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                     std::string_view{"!#$%&'*+-.^_`|~"}.find(static_cast<char>(c)) != std::string_view::npos);
                        }))
            return false;
        if (std::any_of(value.begin(), value.end(), [](unsigned char c) { return (c < 32 && c != '\t') || c == 127; }))
            return false;
    }
    return true;
}

} // namespace

class HttpServer::Impl final
{
public:
    struct Route final
    {
        std::string method;
        std::string shape;
        std::vector<std::string> parts;
        HttpRouteHandler handler;
    };
    struct Mount final
    {
        std::string prefix;
        std::filesystem::path root;
        std::string fallback;
    };

    explicit Impl(HttpServerOptions value) :
        options(std::move(value))
    {
    }

    auto configurable() const -> Result<void>
    {
        if (inNetworkCallback)
            return failure(NetworkError::Internal);
        if (active.load())
            return failure(NetworkError::AlreadyRunning);
        return Result<void>::Success();
    }

    void report(NetworkError code) noexcept
    {
        const auto previous = inNetworkCallback;
        inNetworkCallback   = true;
        defer
        {
            inNetworkCallback = previous;
        };
        try
        {
            if (errorCallback)
                errorCallback(code, {});
        } catch (...)
        {
        } // Observer failures must not recursively invoke the observer.
    }

    auto handleRequest(HttpServerRequest &request) noexcept -> HttpServerResponse
    {
        const auto previous = inNetworkCallback;
        inNetworkCallback   = true;
        defer
        {
            inNetworkCallback = previous;
        };
        try
        {
            auto response = dispatch(request);
            if (validResponse(response))
                return response;
            report(NetworkError::ProtocolError);
        } catch (...)
        {
            report(NetworkError::Internal);
        }
        return HttpServerResponse{.statusCode = 500};
    }

    auto terminal(HttpServerRequest &request, const Route *selected) -> HttpServerResponse
    {
        if (selected)
            return selected->handler(request);
        if (request.method == "GET" || request.method == "HEAD")
        {
            const Mount *mount = nullptr;
            for (const auto &candidate : mounts)
                if (underPrefix(request.path, candidate.prefix) && (!mount || candidate.prefix.size() > mount->prefix.size()))
                    mount = &candidate;
            if (mount)
            {
                auto relative = request.path.substr(mount->prefix == "/" ? 1 : mount->prefix.size());
                if (!relative.empty() && relative.front() == '/')
                    relative.erase(0, 1);
                const auto decoded = decodePath(relative);
                if (!decoded)
                    return {.statusCode = 403};
                auto file = confinedPath(mount->root, *decoded);
                if (!file)
                    return {.statusCode = 403};
                std::error_code error;
                if (std::filesystem::is_directory(*file, error))
                    file = confinedPath(mount->root, (*decoded).empty() ? "index.html" : *decoded + "/index.html");
                if (!file)
                    return {.statusCode = 403};
                if (!SZX::COMMON::FileHelper::Exists(*file) && !mount->fallback.empty())
                    file = confinedPath(mount->root, mount->fallback);
                if (!file)
                    return {.statusCode = 403};
                if (SZX::COMMON::FileHelper::Exists(*file))
                {
                    const auto size = SZX::COMMON::FileHelper::Size(*file);
                    if (!size)
                        return {.statusCode = 500};
                    if (*size > maxStaticBytes)
                        return {.statusCode = 413};
                    const auto bytes = SZX::COMMON::FileHelper::ReadBytes(*file, maxStaticBytes);
                    if (!bytes)
                        return {.statusCode = 500};
                    HttpServerResponse response;
                    response.body.assign(bytes->begin(), bytes->end());
                    response.headers["Content-Type"] = contentType(*file);
                    return response;
                }
            }
        }
        return notFound ? notFound(request) : HttpServerResponse{.statusCode = 404};
    }

    auto dispatch(HttpServerRequest &request) -> HttpServerResponse
    {
        const auto inputParts = segments(request.path);
        const Route *selected = nullptr;
        std::vector<int> bestRank;
        bool methodMismatch = false;
        for (const auto &candidate : routes)
        {
            std::map<std::string, std::string> params;
            std::vector<int> rank;
            bool match    = request.path.starts_with('/');
            std::size_t i = 0;
            for (; i < candidate.parts.size(); ++i)
            {
                const auto &part = candidate.parts[i];
                if (part == "*")
                {
                    rank.push_back(0);
                    break;
                }
                if (i >= inputParts.size())
                {
                    match = false;
                    break;
                }
                if (part.starts_with('{'))
                {
                    const auto decoded = decodePath(inputParts[i]);
                    if (!decoded || decoded->empty())
                    {
                        match = false;
                        break;
                    }
                    params.emplace(part.substr(1, part.size() - 2), *decoded);
                    rank.push_back(1);
                }
                else if (part == inputParts[i])
                    rank.push_back(2);
                else
                {
                    match = false;
                    break;
                }
            }
            if (i == candidate.parts.size() && i != inputParts.size())
                match = false;
            if (!match)
                continue;
            const bool exactMethod = candidate.method == request.method;
            if (!exactMethod && !(request.method == "HEAD" && candidate.method == "GET"))
            {
                methodMismatch = true;
                continue;
            }
            rank.push_back(exactMethod ? 2 : 1);
            if (!selected || rank > bestRank)
            {
                selected           = &candidate;
                bestRank           = std::move(rank);
                request.pathParams = std::move(params);
            }
        }
        HttpNext next = [&, selected](HttpServerRequest &input)
        {
            if (!selected && methodMismatch)
                return HttpServerResponse{.statusCode = 405};
            return terminal(input, selected);
        };
        for (auto it = middleware.rbegin(); it != middleware.rend(); ++it)
        {
            next = [&, layer = &*it, inner = std::move(next)](HttpServerRequest &input)
            {
                return (*layer)(input, inner);
            };
        }
        return next(request);
    }

    HttpServerOptions options;
    std::vector<Route> routes;
    std::vector<Mount> mounts;
    std::vector<HttpMiddleware> middleware;
    HttpRouteHandler notFound;
    ErrorCallback errorCallback;
    std::atomic<bool> active{false};
    std::atomic<std::uint16_t> boundPort{0};
    std::unique_ptr<detail::HttpTransport> transport; // Stop before releasing callback storage.
};

HttpServer::HttpServer(std::unique_ptr<Impl> impl) noexcept :
    impl_(std::move(impl))
{
}
HttpServer::~HttpServer()
{
    // Destruction on an IO callback violates the ownership precondition and cannot join safely.
    if (inNetworkCallback)
        std::terminate();
    (void)stop();
}

auto HttpServer::Create(HttpServerOptions options) -> Result<std::unique_ptr<HttpServer>>
{
    return boundary(
        [&]() -> Result<std::unique_ptr<HttpServer>>
        {
            if (options.listen.host.empty())
                options.listen.host = "127.0.0.1";
            if (options.listen.host.size() >= 64 || options.listen.host.find('\0') != options.listen.host.npos ||
                options.ioThreadCount == 0 || options.ioThreadCount > 64 || options.maxBodyBytes == 0 ||
                options.keepaliveTimeoutMs == 0 || options.keepaliveTimeoutMs > INT_MAX)
                return failure<std::unique_ptr<HttpServer>>(NetworkError::InvalidArgument);
            if (options.tls.enabled)
            {
#if !SZX_NETWORK_HAS_TLS
                return failure<std::unique_ptr<HttpServer>>(NetworkError::TlsUnavailable);
#else
                if (options.tls.certFile.empty() || options.tls.keyFile.empty())
                    return failure<std::unique_ptr<HttpServer>>(NetworkError::InvalidArgument);
#endif
            }
            auto impl = std::make_unique<Impl>(std::move(options));
            return Result<std::unique_ptr<HttpServer>>::Success(std::unique_ptr<HttpServer>(new HttpServer(std::move(impl))));
        });
}

auto HttpServer::route(std::string method, std::string path, HttpRouteHandler handler) -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            const auto shape = routeShape(path);
            if (!validMethod(method) || !shape || !handler)
                return failure(NetworkError::InvalidArgument);
            for (const auto &entry : impl_->routes)
                if (entry.method == method && entry.shape == *shape)
                    return failure(NetworkError::RouteConflict);
            impl_->routes.push_back({std::move(method), *shape, segments(path), std::move(handler)});
            return Result<void>::Success();
        });
}

auto HttpServer::get(std::string path, HttpRouteHandler handler) -> Result<void>
{
    return route("GET", std::move(path), std::move(handler));
}
auto HttpServer::post(std::string path, HttpRouteHandler handler) -> Result<void>
{
    return route("POST", std::move(path), std::move(handler));
}
auto HttpServer::put(std::string path, HttpRouteHandler handler) -> Result<void>
{
    return route("PUT", std::move(path), std::move(handler));
}
auto HttpServer::del(std::string path, HttpRouteHandler handler) -> Result<void>
{
    return route("DELETE", std::move(path), std::move(handler));
}

auto HttpServer::mountStatic(std::string prefix, std::filesystem::path directory, std::string fallback) -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            if (!validPath(prefix) || prefix.find_first_of("{}*") != prefix.npos || directory.empty())
                return failure(NetworkError::InvalidArgument);
            if (prefix.size() > 1 && prefix.back() == '/')
                prefix.pop_back();
            for (const auto &part : segments(prefix))
                if (part == "." || part == "..")
                    return failure(NetworkError::InvalidArgument);
            std::error_code error;
            auto root = std::filesystem::canonical(directory, error);
            if (error || !std::filesystem::is_directory(root, error))
                return failure(NetworkError::InvalidArgument);
            if (!fallback.empty())
            {
                const auto file = confinedPath(root, fallback);
                if (!file || !SZX::COMMON::FileHelper::Exists(*file))
                    return failure(NetworkError::InvalidArgument);
            }
            for (const auto &mount : impl_->mounts)
                if (mount.prefix == prefix)
                    return failure(NetworkError::RouteConflict);
            impl_->mounts.push_back({std::move(prefix), std::move(root), std::move(fallback)});
            return Result<void>::Success();
        });
}

auto HttpServer::use(HttpMiddleware middleware) -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            if (!middleware)
                return failure(NetworkError::InvalidArgument);
            impl_->middleware.push_back(std::move(middleware));
            return Result<void>::Success();
        });
}
auto HttpServer::onNotFound(HttpRouteHandler handler) -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            if (!handler)
                return failure(NetworkError::InvalidArgument);
            impl_->notFound = std::move(handler);
            return Result<void>::Success();
        });
}
auto HttpServer::onError(ErrorCallback callback) -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            impl_->errorCallback = std::move(callback);
            return Result<void>::Success();
        });
}

auto HttpServer::start() -> Result<void>
{
    return boundary(
        [&]() -> Result<void>
        {
            const auto state = impl_->configurable();
            if (!state.ok)
                return state;
            auto transport = std::make_unique<detail::HttpTransport>(
                impl_->options, [this](HttpServerRequest &request) { return impl_->handleRequest(request); },
                [this](NetworkError error) { impl_->report(error); });
            const auto started = transport->start();
            if (!started.ok)
                return Result<void>::Failure(started.error, started.message);
            impl_->transport = std::move(transport);
            impl_->boundPort.store(started.value);
            impl_->active.store(true);
            return Result<void>::Success();
        });
}

auto HttpServer::stop() -> Result<void>
{
    if (inNetworkCallback)
        return failure(NetworkError::Internal);
    return boundary(
        [&]() -> Result<void>
        {
            if (!impl_->active.load())
                return Result<void>::Success();
            impl_->transport->stop();
            impl_->transport.reset();
            impl_->boundPort.store(0);
            impl_->active.store(false);
            return Result<void>::Success();
        });
}
auto HttpServer::running() const noexcept -> bool
{
    return impl_->active.load();
}
auto HttpServer::port() const noexcept -> std::uint16_t
{
    return impl_->boundPort.load();
}
} // namespace SZX::NETWORK::v1
