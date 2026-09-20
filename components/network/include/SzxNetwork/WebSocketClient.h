#pragma once
#include "SzxNetwork/WebSocketTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
struct WebSocketClientOptions final
{
    std::string url;
    HttpHeaders headers;
    std::uint32_t connectTimeoutMs = 5000;          ///< TCP connect only, excludes DNS and HTTP upgrade.
    std::uint32_t maxMessageBytes  = 1024u * 1024u; ///< Checked before buffering each frame; includes all message fragments.
    std::uint32_t maxPendingBytes  = 4u * 1024u * 1024u;
};
/** Single-connection ws:// client. start is asynchronous; onConnection(true) means upgrade completed.
 * No TLS, redirects, automatic reconnect, compression or custom subprotocols. stop then start allows reuse.
 * Thread, ownership and callback rules match WebSocketServer.
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API WebSocketClient final
{
public:
    static auto Create(WebSocketClientOptions options, std::shared_ptr<WebSocketClientCallbacks> callbacks = {})
        -> SZX::COMMON::Result<std::unique_ptr<WebSocketClient>>;
    ~WebSocketClient();
    WebSocketClient(const WebSocketClient &)                     = delete;
    auto operator=(const WebSocketClient &) -> WebSocketClient & = delete;
    auto start() -> SZX::COMMON::Result<void>;
    auto stop() -> SZX::COMMON::Result<void>;
    auto isConnected() const noexcept -> bool;
    auto send(std::string data, WebSocketMessageType type = WebSocketMessageType::Text) -> SZX::COMMON::Result<void>;
    auto close() -> SZX::COMMON::Result<void>;
    auto setContext(WebSocketContext context) -> SZX::COMMON::Result<void>;
    auto context() const -> SZX::COMMON::Result<WebSocketContext>;

private:
    class Impl;
    explicit WebSocketClient(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
