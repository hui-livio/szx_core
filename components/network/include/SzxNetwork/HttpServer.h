#pragma once
/** @file HttpServer.h
 * @brief Synchronous HTTP/1.1 server facade, public API version 1.
 */
#include <cstddef>
#include <filesystem>
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief Owned request values, valid for the synchronous middleware/handler invocation. */
struct HttpServerRequest final
{
    std::string method;
    std::string path; ///< URL path without query, preserving percent encoding.
    std::string body; ///< Binary-safe, bounded by maxBodyBytes.
    HttpHeaders headers;
    std::map<std::string, std::string> query;      ///< Decoded query parameters, case-sensitive.
    std::map<std::string, std::string> pathParams; ///< Decoded {name} captures, separate from query.
    Endpoint peer;
    std::map<std::string, std::string> attributes; ///< Initially empty; only application middleware writes it.
    std::shared_ptr<void> userContext;             ///< Initially empty; application-owned request context.
};
/** @brief HTTP response; invalid status or header fields become HTTP 500. */
struct HttpServerResponse final
{
    int statusCode = 200; ///< Final status, 200..599; informational/upgrade responses are unsupported.
    std::string body;
    HttpHeaders headers; ///< Content-Length and transfer framing are owned by the server.
};
using HttpRouteHandler = std::function<HttpServerResponse(const HttpServerRequest &)>;
using HttpNext         = std::function<HttpServerResponse(HttpServerRequest &)>;
/** @brief First registered middleware is outermost; next must be called synchronously, at most once. */
using HttpMiddleware = std::function<HttpServerResponse(HttpServerRequest &, const HttpNext &)>;
/** @brief Customize an error response. The request may be incomplete; its status cannot be changed. */
using HttpRequestErrorHandler = std::function<HttpServerResponse(const HttpServerRequest &, int, NetworkError)>;

/** @brief HTTP server settings. The first release owns its IO threads; no shared loops. */
struct HttpServerOptions final
{
    Endpoint listen;
    std::size_t ioThreadCount        = 4;                  ///< Must be 1..64.
    std::size_t maxBodyBytes         = 1u * 1024u * 1024u; ///< Nonzero; early 413 for fixed and chunked bodies.
    std::uint32_t keepaliveTimeoutMs = 60000;              ///< Nonzero and <= INT_MAX.
    TlsOptions tls;
};

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
/**
 * @brief Owned HTTP server with no built-in authentication or authorization.
 * @details Configure while stopped. Configuration and lifecycle calls must be serialized by the caller.
 * running()/port() may be read concurrently. Callbacks run on IO threads and may run concurrently;
 * do not perform blocking work or destroy the server from a callback. Captured objects must outlive stop().
 * Exceptions from handlers/middleware become 500 and onError(Internal); onError exceptions are contained.
 * The DLL and client must use matching compiler, STL and runtime settings.
 */
class SZX_NETWORK_API HttpServer final
{
public:
    /** @brief Validate options without opening a socket. Empty host defaults to 127.0.0.1.
     * @param[in] options Server configuration.
     * @return Owned server, InvalidArgument, TlsUnavailable or Internal.
     */
    [[nodiscard]] static auto Create(HttpServerOptions options) -> SZX::COMMON::Result<std::unique_ptr<HttpServer>>;
    /** @brief Create a reusable, confined GET/HEAD file handler for application routing policies.
     * @param[in] directory Existing root directory; keep it immutable to untrusted writers.
     * @param[in] homePage Relative directory index path, confined to directory.
     * @return Owned handler, InvalidArgument or Internal. The handler returns 404 for missing files,
     * 403 for unsafe paths, 413 above 16 MiB, and 500 for read failures or exceptions.
     * @note Paths are relative to URL root. No SPA fallback; safe to invoke concurrently.
     */
    [[nodiscard]] static auto CreateStaticHandler(std::filesystem::path directory, std::string homePage = "index.html")
        -> SZX::COMMON::Result<HttpRouteHandler>;
    /** @brief Stop and join all workers. Must execute outside every network callback. */
    ~HttpServer();
    HttpServer(const HttpServer &)                     = delete;
    auto operator=(const HttpServer &) -> HttpServer & = delete;

    /** @brief Register an uppercase GET/HEAD/POST/PUT/PATCH/DELETE/OPTIONS/TRACE route.
     * @param[in] method HTTP method.
     * @param[in] path Absolute path with whole-segment {name} captures or a terminal /* wildcard.
     * @param[in] handler Synchronous callback.
     * @return InvalidArgument for invalid syntax; RouteConflict for duplicate method/path shapes.
     * @note Literal segments precede parameters, which precede wildcards. Equal specificity uses registration order.
     * HEAD falls back to GET with body suppressed; a matched path with another method returns 405.
     */
    [[nodiscard]] auto route(std::string method, std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Register a GET route. @see route */
    [[nodiscard]] auto get(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Register a POST route. @see route */
    [[nodiscard]] auto post(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Register a PUT route. @see route */
    [[nodiscard]] auto put(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Register a DELETE route. @see route */
    [[nodiscard]] auto del(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;

    /** @brief Mount a directory for GET/HEAD, after explicit routes and inside middleware.
     * @param[in] urlPrefix Absolute literal URL prefix; longest matching mount wins.
     * @param[in] directory Existing directory. Keep its tree immutable to untrusted writers while serving.
     * @param[in] fallbackFile Optional relative existing file, used only for missing files inside this mount.
     * @return InvalidArgument for invalid/escaping paths; RouteConflict for duplicate prefix.
     * @note Rejects traversal, drive/stream paths and symlink escapes. Files are limited to 16 MiB;
     * larger files return 413. Directory requests use index.html. No directory listing.
     */
    [[nodiscard]] auto mountStatic(std::string urlPrefix, std::filesystem::path directory, std::string fallbackFile = {})
        -> SZX::COMMON::Result<void>;
    /** @brief Append onion middleware; returns AlreadyRunning or InvalidArgument when rejected. */
    [[nodiscard]] auto use(HttpMiddleware middleware) -> SZX::COMMON::Result<void>;
    /** @brief Replace the default empty 404 response handler while stopped. */
    [[nodiscard]] auto onNotFound(HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Set the error observer while stopped; an empty observer disables notification.
     * @note Receives callback/protocol/body-limit failures; synchronous API failures are returned directly.
     */
    [[nodiscard]] auto onError(ErrorCallback callback) -> SZX::COMMON::Result<void>;
    /** @brief Customize parser/body-limit and uncaught handler/middleware error responses.
     * @param[in] handler Callback, or empty to restore empty error responses.
     * @return Success, AlreadyRunning or Internal.
     * @note Runs outside middleware, under the same callback lifetime rules. Partial request data
     * is untrusted. The original HTTP status and connection framing are preserved. Throwing or
     * returning invalid headers falls back to an empty response; onError still observes the error.
     */
    [[nodiscard]] auto onRequestError(HttpRequestErrorHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief Set the total accepted connection limit while stopped.
     * @param[in] count Nonzero limit; excess connections are closed before request processing.
     * @return Success, InvalidArgument, AlreadyRunning or Internal.
     * @note The default remains ioThreadCount * 1024. Includes idle keepalive connections.
     */
    [[nodiscard]] auto setMaxConnections(std::uint32_t count) -> SZX::COMMON::Result<void>;
    /** @brief Bind and start IO workers; returns BindFailed or AlreadyRunning on failure. */
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    /** @brief Stop/join workers and close connections. Idempotent; restart retains configuration.
     * @return Success guarantees no further callbacks. Internal if called from any network callback.
     * @note In-flight handlers finish, but queued network responses are not guaranteed delivery.
     */
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    /** @brief Whether start succeeded and stop has not completed. */
    [[nodiscard]] auto running() const noexcept -> bool;
    /** @brief Actual bound port while running, or zero when stopped. */
    [[nodiscard]] auto port() const noexcept -> std::uint16_t;

private:
    class Impl;
    explicit HttpServer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
