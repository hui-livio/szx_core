#pragma once
#include "SzxNetwork/WebSocketTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
struct WebSocketServerOptions final
{
    Endpoint listen;
    std::uint32_t maxConnections  = 1024;
    std::uint32_t maxMessageBytes = 1024u * 1024u;      ///< Checked before buffering each frame; includes all message fragments.
    std::uint32_t maxPendingBytes = 4u * 1024u * 1024u; ///< Each of the task queue and hv write buffer is bounded separately.
};
/** ws:// server, one hv worker thread. No TLS, routing, authentication or subprotocol negotiation policy.
 * Lifecycle calls must be serialized; destruction cannot overlap public calls. Other operations may be concurrent.
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API WebSocketServer final
{
public:
    static auto Create(WebSocketServerOptions options = {}, std::shared_ptr<WebSocketServerCallbacks> callbacks = {})
        -> SZX::COMMON::Result<std::unique_ptr<WebSocketServer>>;
    ~WebSocketServer();
    WebSocketServer(const WebSocketServer &)                     = delete;
    auto operator=(const WebSocketServer &) -> WebSocketServer & = delete;
    auto start() -> SZX::COMMON::Result<void>;
    auto stop() -> SZX::COMMON::Result<void>;
    auto running() const noexcept -> bool;
    auto port() const noexcept -> std::uint16_t;
    auto getChannelById(WebSocketConnectionId id) const -> SZX::COMMON::Result<WebSocketHandle>;
    auto connectionNum() const -> SZX::COMMON::Result<std::size_t>;
    /** Visits a snapshot without holding the table lock, on the caller thread. No lifecycle operations in visitor. */
    auto foreachChannel(WebSocketChannelVisitor &visitor) -> SZX::COMMON::Result<void>;
    /** Delegates to each handle; stops submitting at first failure, no rollback. */
    auto broadcast(std::string data, WebSocketMessageType type = WebSocketMessageType::Text) -> SZX::COMMON::Result<void>;

private:
    class Impl;
    explicit WebSocketServer(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
