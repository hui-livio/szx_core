#pragma once
/** @file NetworkError.h
 * @brief Stable network error identities; reserved values match the network design.
 */
#include <functional>
#include <string>

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief Module-local codes stored in COMMON::Result::error. Never renumber. */
enum class NetworkError : int
{
    InvalidArgument    = 1,  ///< Invalid options, route syntax, callback or static path.
    NotInitialized     = 2,  ///< Reserved for operations requiring a running endpoint.
    AlreadyRunning     = 3,  ///< Start or configuration attempted while running.
    ResolveFailed      = 4,  ///< Reserved for DNS resolution.
    ConnectFailed      = 5,  ///< Reserved for client connections.
    ConnectTimeout     = 6,  ///< Reserved for client timeouts.
    BindFailed         = 7,  ///< Listening socket could not be created or bound.
    Closed             = 8,  ///< Reserved for closed connections.
    SendFailed         = 9,  ///< Reserved for transport sends.
    SendQueueFull      = 10, ///< Reserved for transport backpressure.
    ReadTimeout        = 11, ///< Reserved for read deadlines.
    MessageTooLarge    = 12, ///< Request body exceeded the configured maximum (HTTP 413).
    ProtocolError      = 13, ///< Invalid handler response (HTTP 500).
    RouteConflict      = 14, ///< Duplicate method/path shape or static prefix.
    TlsUnavailable     = 15, ///< TLS requested in a build without TLS support.
    TlsHandshakeFailed = 16, ///< TLS context/certificate initialization failed.
    Cancelled          = 17, ///< Reserved for cancelled asynchronous operations.
    Internal           = 18  ///< Unexpected exception or blocking lifecycle call from a callback.
};
/** @brief Called on the request's IO thread; messages are sanitized, never exception text. */
using ErrorCallback = std::function<void(NetworkError, const std::string &)>;
} // namespace v1
} // namespace SZX::NETWORK
