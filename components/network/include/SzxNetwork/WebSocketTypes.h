#pragma once
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
using WebSocketConnectionId = std::uint64_t;
using WebSocketContext      = std::shared_ptr<void>;
enum class WebSocketMessageType
{
    Text,
    Binary
};
struct WebSocketState;
/** Copyable server connection, non-owning with respect to its endpoint.
 * All operations return Closed after connection removal. Context contents require application synchronization.
 * send accepts a complete message; success means queued, not delivered. close sends normal close then closes transport.
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API WebSocketHandle final
{
public:
    WebSocketHandle() = default;
    auto id() const noexcept -> WebSocketConnectionId
    {
        return id_;
    }
    auto isConnected() const noexcept -> bool;
    auto peeraddr() const -> SZX::COMMON::Result<Endpoint>;
    auto path() const -> SZX::COMMON::Result<std::string>;
    auto send(std::string data, WebSocketMessageType type = WebSocketMessageType::Text) const -> SZX::COMMON::Result<void>;
    auto close() const -> SZX::COMMON::Result<void>;
    auto setContext(WebSocketContext context) const -> SZX::COMMON::Result<void>;
    auto context() const -> SZX::COMMON::Result<WebSocketContext>;
    template <typename T>
    auto getContextPtr() const -> SZX::COMMON::Result<std::shared_ptr<T>>
    {
        auto value = context();
        if (!value.ok)
            return SZX::COMMON::Result<std::shared_ptr<T>>::Failure(value.error, {});
        return SZX::COMMON::Result<std::shared_ptr<T>>::Success(std::static_pointer_cast<T>(value.value));
    }

private:
    friend struct WebSocketState;
    WebSocketHandle(std::weak_ptr<WebSocketState> state, WebSocketConnectionId id) :
        state_(std::move(state)),
        id_(id)
    {
    }
    std::weak_ptr<WebSocketState> state_;
    WebSocketConnectionId id_{};
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
/** Events run on the endpoint IO thread. Views last only for the callback.
 * Do not block, start/stop or destroy endpoints in callbacks. send/close/context are allowed.
 * Exceptions cause Internal and connection closure; exceptions from onError are contained.
 * Endpoints hold the shared callback object until destruction. Stop joins callbacks.
 */
struct WebSocketServerCallbacks
{
    virtual ~WebSocketServerCallbacks()                                                     = default;
    virtual void onConnection(const WebSocketHandle &)                                      = 0;
    virtual void onMessage(const WebSocketHandle &, std::string_view, WebSocketMessageType) = 0;
    virtual void onError(const WebSocketHandle &, NetworkError)                             = 0;
};
struct WebSocketClientCallbacks
{
    virtual ~WebSocketClientCallbacks()                            = default;
    virtual void onConnection(bool connected)                      = 0;
    virtual void onMessage(std::string_view, WebSocketMessageType) = 0;
    virtual void onError(NetworkError)                             = 0;
};
struct WebSocketChannelVisitor
{
    virtual ~WebSocketChannelVisitor()              = default;
    virtual void onChannel(const WebSocketHandle &) = 0;
};
} // namespace v1
} // namespace SZX::NETWORK
