
#include <atomic>
#include <charconv>
#include <climits>
#include <hv/TcpClient.h>
#include <hv/TcpServer.h>
#include <hv/herr.h>
#include <map>
#include <mutex>
#include <variant>
#include <vector>
#include "Defer.hpp"
#include "SzxNetwork/WebSocketClient.h"
#include "SzxNetwork/WebSocketServer.h"
#include "detail/WebSocketProtocol.h"

namespace SZX::NETWORK::v1
{
namespace
{
using Void                            = SZX::COMMON::Result<void>;
thread_local bool inWebSocketCallback = false;
struct CallbackScope
{
    bool previous = inWebSocketCallback;
    CallbackScope()
    {
        inWebSocketCallback = true;
    }
    ~CallbackScope()
    {
        inWebSocketCallback = previous;
    }
};
template <typename T = void>
auto failure(NetworkError code) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(code), {});
}
template <typename F>
auto boundary(F &&fn) -> decltype(fn())
{
    try
    {
        return fn();
    } catch (const std::bad_alloc &)
    {
        return decltype(fn())::Failure(static_cast<int>(NetworkError::ResourceExhausted), {});
    } catch (...)
    {
        return decltype(fn())::Failure(static_cast<int>(NetworkError::Internal), {});
    }
}
bool validLimits(std::uint32_t message, std::uint32_t pending)
{
    return message && message <= 16u * 1024u * 1024u && pending >= 128 && pending <= INT_MAX / 2;
}
bool validType(WebSocketMessageType type)
{
    return type == WebSocketMessageType::Text || type == WebSocketMessageType::Binary;
}
struct WsEntry
{
    WebSocketChannelPtr channel;
    hv::EventLoopPtr loop;
    Endpoint peer;
    std::string path;
    WebSocketContext context;
    std::size_t pending{};
    bool closing{};
};
} // namespace
struct WebSocketState : std::enable_shared_from_this<WebSocketState>
{
    using Callbacks = std::variant<std::shared_ptr<WebSocketServerCallbacks>, std::shared_ptr<WebSocketClientCallbacks>>;
    explicit WebSocketState(std::uint32_t message, std::uint32_t pending, Callbacks cb) :
        maxMessage(message),
        maxPending(pending),
        callbacks(std::move(cb))
    {
    }
    std::uint32_t maxMessage, maxPending;
    Callbacks callbacks;
    mutable std::mutex gate;
    std::map<WebSocketConnectionId, std::shared_ptr<WsEntry>> entries;
    std::atomic_bool active{};
    WebSocketConnectionId nextId = 1, current = 0;
    WebSocketHandle handle(WebSocketConnectionId id)
    {
        return {weak_from_this(), id};
    }
    template <typename F>
    void notify(F &&fn)
    {
        CallbackScope scope;
        std::visit(
            [&](const auto &cb)
            {
                if (cb)
                    fn(*cb);
            },
            callbacks);
    }
    void report(WebSocketConnectionId id, NetworkError code) noexcept
    {
        try
        {
            notify(
                [&](auto &cb)
                {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(cb)>, WebSocketServerCallbacks>)
                        cb.onError(handle(id), code);
                    else
                        cb.onError(code);
                });
        } catch (...)
        {
        }
    }
    WebSocketConnectionId find(const WebSocketChannelPtr &channel) const
    {
        for (const auto &[id, entry] : entries)
            if (entry->channel == channel)
                return id;
        return 0;
    }
    void connectionEvent(WebSocketConnectionId id, bool connected)
    {
        notify(
            [&](auto &cb)
            {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(cb)>, WebSocketServerCallbacks>)
                    cb.onConnection(handle(id));
                else
                    cb.onConnection(connected);
            });
    }
    void closeError(WebSocketConnectionId id, const WebSocketChannelPtr &channel, NetworkError code) noexcept
    {
        try
        {
            {
                std::lock_guard lock(gate);
                auto it = entries.find(id);
                if (it != entries.end())
                    it->second->closing = true;
            }
            report(id, code);
            channel->SocketChannel::close(true);
        } catch (...)
        {
            report(id, NetworkError::Internal);
        }
    }
    void opened(const WebSocketChannelPtr &channel, hv::EventLoopPtr loop, std::string_view path) noexcept
    {
        CallbackScope scope;
        WebSocketConnectionId id = 0;
        try
        {
            auto entry     = std::make_shared<WsEntry>();
            entry->channel = channel;
            entry->loop    = std::move(loop);
            entry->path    = path;
            char ip[64]{};
            auto address = reinterpret_cast<sockaddr_u *>(hio_peeraddr(channel->io()));
            sockaddr_ip(address, ip, sizeof(ip));
            entry->peer = {ip, sockaddr_port(address)};
            channel->setMaxWriteBufsize(maxPending);
            {
                std::lock_guard lock(gate);
                if (!active)
                {
                    channel->SocketChannel::close(true);
                    return;
                }
                id = nextId++;
                entries.emplace(id, std::move(entry));
                current = id;
            }
            connectionEvent(id, true);
        } catch (...)
        {
            closeError(id, channel, NetworkError::Internal);
        }
    }
    void closed(const WebSocketChannelPtr &channel, NetworkError unopenedError = NetworkError::ConnectFailed) noexcept
    {
        CallbackScope scope;
        WebSocketConnectionId id = 0;
        std::shared_ptr<WsEntry> retired;
        try
        {
            {
                std::lock_guard lock(gate);
                id = find(channel);
                if (id)
                {
                    retired          = entries.at(id);
                    retired->closing = true;
                }
                if (current == id)
                    current = 0;
            }
            defer
            {
                std::lock_guard lock(gate);
                entries.erase(id);
            };
            if (active)
            {
                if (!id)
                    report(0, unopenedError);
                else if (channel->error())
                    report(id, NetworkError::Closed);
                connectionEvent(id, false);
            }
        } catch (...)
        {
            report(id, NetworkError::Internal);
        }
    }
    void received(const WebSocketChannelPtr &channel, const std::string &data) noexcept
    {
        CallbackScope scope;
        WebSocketConnectionId id = 0;
        try
        {
            {
                std::lock_guard lock(gate);
                id = find(channel);
                if (!active || !id || entries.at(id)->closing)
                    return;
            }
            if (data.size() > maxMessage)
            {
                closeError(id, channel, NetworkError::MessageTooLarge);
                return;
            }
            const auto type = channel->opcode == WS_OPCODE_BINARY ? WebSocketMessageType::Binary : WebSocketMessageType::Text;
            notify(
                [&](auto &cb)
                {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(cb)>, WebSocketServerCallbacks>)
                        cb.onMessage(handle(id), data, type);
                    else
                        cb.onMessage(data, type);
                });
        } catch (...)
        {
            closeError(id, channel, NetworkError::Internal);
        }
    }
    auto attach(const WebSocketChannelPtr &channel, const hv::EventLoopPtr &loop) -> std::shared_ptr<detail::WebSocketProtocol>
    {
        const std::weak_ptr<hv::WebSocketChannel> weak = channel;
        auto protocol                                  = std::make_shared<detail::WebSocketProtocol>(
            *channel, maxMessage, maxPending,
            detail::WebSocketProtocol::Callbacks{[this, weak, loop](std::string_view path) noexcept
                                                 {
                                                     if (auto connected = weak.lock())
                                                         opened(connected, loop, path);
                                                 },
                                                 [this, weak](const std::string &data) noexcept
                                                 {
                                                     if (auto connected = weak.lock())
                                                         received(connected, data);
                                                 },
                                                 [this, weak](NetworkError code) noexcept
                                                 {
                                                     if (auto connected = weak.lock())
                                                     {
                                                         try
                                                         {
                                                             WebSocketConnectionId id;
                                                             {
                                                                 std::lock_guard lock(gate);
                                                                 id = find(connected);
                                                             }
                                                             closeError(id, connected, code);
                                                         } catch (...)
                                                         {
                                                             connected->SocketChannel::close(true);
                                                         }
                                                     }
                                                 }});
        channel->setContextPtr(protocol);
        return protocol;
    }
    static void receiveBytes(const WebSocketChannelPtr &channel, hv::Buffer *buffer) noexcept
    {
        if (const auto protocol = channel->getContextPtr<detail::WebSocketProtocol>())
            protocol->receive(static_cast<const char *>(buffer->data()), buffer->size());
    }
    // Caller holds gate. Tasks run on the channel's IO loop, including hv framing and write-buffer access.
    auto send(WebSocketConnectionId id, std::string data, WebSocketMessageType type) -> Void
    {
        if (!active)
            return failure(NetworkError::Closed);
        auto it = entries.find(id);
        if (it == entries.end() || it->second->closing)
            return failure(NetworkError::Closed);
        if (!validType(type))
            return failure(NetworkError::InvalidArgument);
        if (data.size() > maxMessage)
            return failure(NetworkError::MessageTooLarge);
        const auto cost  = data.size() + 14 * (data.size() / 65535 + 1);
        const auto entry = it->second;
        if (cost > maxPending || entry->pending > maxPending - cost)
            return failure(NetworkError::SendQueueFull);
        entry->pending += cost;
        bool queued     = false;
        defer
        {
            if (!queued)
                entry->pending -= cost;
        };
        entry->loop->queueInLoop(
            [this, id, weak = std::weak_ptr<WsEntry>(entry), cost, data = std::move(data), type]() noexcept
            {
                const auto entry = weak.lock();
                if (!entry)
                    return;
                try
                {
                    {
                        std::lock_guard lock(gate);
                        entry->pending -= cost;
                        if (!active)
                            return;
                    }
                    if (!entry->channel->isConnected())
                        return;
                    if (entry->channel->writeBufsize() > maxPending - cost)
                    {
                        closeError(id, entry->channel, NetworkError::SendQueueFull);
                        return;
                    }
                    if (entry->channel->send(data, type == WebSocketMessageType::Text ? WS_OPCODE_TEXT : WS_OPCODE_BINARY) < 0)
                        closeError(id, entry->channel, NetworkError::SendFailed);
                } catch (...)
                {
                    closeError(id, entry->channel, NetworkError::Internal);
                }
            });
        queued = true;
        return Void::Success();
    }
    auto close(WebSocketConnectionId id) -> Void
    {
        if (!active)
            return failure(NetworkError::Closed);
        auto it = entries.find(id);
        if (it == entries.end())
            return failure(NetworkError::Closed);
        auto entry = it->second;
        if (!entry->closing)
        {
            entry->loop->queueInLoop(
                [this, id, weak = std::weak_ptr<WsEntry>(entry)]() noexcept
                {
                    if (auto entry = weak.lock())
                    {
                        try
                        {
                            if (entry->channel->isConnected())
                                entry->channel->send(std::string("\x03\xe8", 2), WS_OPCODE_CLOSE);
                            entry->channel->SocketChannel::close(true);
                        } catch (...)
                        {
                            closeError(id, entry->channel, NetworkError::Internal);
                        }
                    }
                });
            entry->closing = true;
        }
        return Void::Success();
    }
    auto setContext(WebSocketConnectionId id, WebSocketContext &value) -> Void
    {
        auto it = entries.find(id);
        if (!active || it == entries.end())
            return failure(NetworkError::Closed);
        it->second->context.swap(value);
        return Void::Success();
    }
    auto context(WebSocketConnectionId id) const -> SZX::COMMON::Result<WebSocketContext>
    {
        auto it = entries.find(id);
        if (!active || it == entries.end())
            return failure<WebSocketContext>(NetworkError::Closed);
        return SZX::COMMON::Result<WebSocketContext>::Success(it->second->context);
    }
    void clear()
    {
        decltype(entries) retired;
        {
            std::lock_guard lock(gate);
            retired.swap(entries);
            current = 0;
        }
    }
};

class WebSocketServer::Impl : public WebSocketState
{
public:
    Impl(WebSocketServerOptions value, std::shared_ptr<WebSocketServerCallbacks> cb) :
        WebSocketState(value.maxMessageBytes, value.maxPendingBytes, std::move(cb)),
        options(std::move(value))
    {
    }
    WebSocketServerOptions options;
    std::atomic_uint16_t port{};
    std::unique_ptr<hv::TcpServerTmpl<hv::WebSocketChannel>> backend;
};
class WebSocketClient::Impl : public WebSocketState
{
public:
    Impl(WebSocketClientOptions value, std::shared_ptr<WebSocketClientCallbacks> cb) :
        WebSocketState(value.maxMessageBytes, value.maxPendingBytes, std::move(cb)),
        options(std::move(value))
    {
    }
    WebSocketClientOptions options;
    std::unique_ptr<hv::TcpClientTmpl<hv::WebSocketChannel>> backend;
};

WebSocketServer::WebSocketServer(std::shared_ptr<Impl> value) :
    impl_(std::move(value))
{
}
WebSocketClient::WebSocketClient(std::shared_ptr<Impl> value) :
    impl_(std::move(value))
{
}
WebSocketServer::~WebSocketServer()
{
    (void)stop();
}
WebSocketClient::~WebSocketClient()
{
    (void)stop();
}

auto WebSocketServer::Create(WebSocketServerOptions options, std::shared_ptr<WebSocketServerCallbacks> callbacks)
    -> SZX::COMMON::Result<std::unique_ptr<WebSocketServer>>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::unique_ptr<WebSocketServer>>
        {
            if (!validLimits(options.maxMessageBytes, options.maxPendingBytes) || !options.maxConnections ||
                options.listen.host.size() >= 64 || options.listen.host.find('\0') != std::string::npos)
                return failure<std::unique_ptr<WebSocketServer>>(NetworkError::InvalidArgument);
            if (options.listen.host.empty())
                options.listen.host = "127.0.0.1";
            return SZX::COMMON::Result<std::unique_ptr<WebSocketServer>>::Success(std::unique_ptr<WebSocketServer>(
                new WebSocketServer(std::make_shared<Impl>(std::move(options), std::move(callbacks)))));
        });
}
auto WebSocketClient::Create(WebSocketClientOptions options, std::shared_ptr<WebSocketClientCallbacks> callbacks)
    -> SZX::COMMON::Result<std::unique_ptr<WebSocketClient>>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::unique_ptr<WebSocketClient>>
        {
            if (options.url.starts_with("wss://"))
                return failure<std::unique_ptr<WebSocketClient>>(NetworkError::TlsUnavailable);
            if (!options.url.starts_with("ws://") || options.url.find_first_of("\r\n\t #") != std::string::npos ||
                options.url.find('\0') != std::string::npos || options.url.find('@') != std::string::npos ||
                !validLimits(options.maxMessageBytes, options.maxPendingBytes) || !options.connectTimeoutMs ||
                options.connectTimeoutMs > INT_MAX)
                return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            if (options.url.size() > 8192)
                return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            HttpRequest request;
            request.url = "http" + options.url.substr(2);
            hurl_t parsed{};
            if (hv_parse_url(&parsed, request.url.c_str()) != 0 || !parsed.fields[HV_URL_HOST].len ||
                (parsed.fields[HV_URL_PORT].len && parsed.port == 0))
                return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            if (parsed.fields[HV_URL_PORT].len)
            {
                const auto field     = parsed.fields[HV_URL_PORT];
                const auto *begin    = request.url.data() + field.off;
                const auto *end      = begin + field.len;
                unsigned port        = 0;
                const auto converted = std::from_chars(begin, end, port);
                if (converted.ec != std::errc{} || converted.ptr != end || !port || port > 65535)
                    return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            }
            request.ParseUrl();
            if (request.host.empty() || !request.port || request.port > 65535)
                return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            const CaseInsensitiveLess less;
            for (const auto &[key, value] : options.headers)
            {
                if (key.empty() ||
                    key.find_first_not_of("!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") !=
                        std::string::npos ||
                    value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos)
                    return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
                for (const auto reserved :
                     {"Host", "Connection", "Upgrade", "Content-Length", "Transfer-Encoding", "Sec-WebSocket-Key",
                      "Sec-WebSocket-Version", "Sec-WebSocket-Extensions", "Sec-WebSocket-Protocol"})
                    if (!less(key, reserved) && !less(reserved, key))
                        return failure<std::unique_ptr<WebSocketClient>>(NetworkError::InvalidArgument);
            }
            return SZX::COMMON::Result<std::unique_ptr<WebSocketClient>>::Success(std::unique_ptr<WebSocketClient>(
                new WebSocketClient(std::make_shared<Impl>(std::move(options), std::move(callbacks)))));
        });
}
auto WebSocketServer::start() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inWebSocketCallback)
                return failure(NetworkError::Internal);
            if (impl_->active)
                return failure(NetworkError::AlreadyRunning);
            auto *state    = impl_.get();
            impl_->backend = std::make_unique<hv::TcpServerTmpl<hv::WebSocketChannel>>();
            // hv's acceptor is also the IO loop when there are no worker threads.
            impl_->backend->setThreadNum(0);
            impl_->backend->setMaxConnectionNum(impl_->options.maxConnections);
            impl_->backend->onConnection = [state](const WebSocketChannelPtr &channel) noexcept
            {
                try
                {
                    if (channel->isConnected())
                    {
                        channel->type = WS_SERVER;
                        state->attach(channel, state->backend->loop());
                    }
                    else if (const auto protocol = channel->getContextPtr<detail::WebSocketProtocol>();
                             protocol && protocol->ready())
                        state->closed(channel);
                } catch (...)
                {
                    channel->SocketChannel::close(true);
                }
            };
            impl_->backend->onMessage = WebSocketState::receiveBytes;
            const int fd = impl_->backend->createsocket(impl_->options.listen.port, impl_->options.listen.host.c_str());
            if (fd < 0)
                return failure(NetworkError::BindFailed);
            bool owned = true;
            defer
            {
                if (owned)
                {
                    impl_->backend->listenfd = -1;
                    ::closesocket(fd);
                }
            };
            sockaddr_u address{};
            socklen_t size = sizeof(address);
            if (getsockname(fd, &address.sa, &size) != 0)
                return failure(NetworkError::BindFailed);
            // Let the loop own the descriptor before starting the acceptor thread.
            if (!hio_get(impl_->backend->loop()->loop(), fd))
                return failure(NetworkError::Internal);
            owned         = false;
            impl_->active = true;
            bool started  = false;
            defer
            {
                if (!started)
                {
                    impl_->active = false;
                    impl_->backend->stop();
                    impl_->clear();
                }
            };
            impl_->backend->start();
            impl_->port = sockaddr_port(&address);
            started     = true;
            return Void::Success();
        });
}
auto WebSocketClient::start() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inWebSocketCallback)
                return failure(NetworkError::Internal);
            if (impl_->active)
                return failure(NetworkError::AlreadyRunning);
            auto *state    = impl_.get();
            impl_->backend = std::make_unique<hv::TcpClientTmpl<hv::WebSocketChannel>>();
            auto &backend  = *impl_->backend;
            backend.setConnectTimeout(static_cast<int>(impl_->options.connectTimeoutMs));
            auto request = std::make_shared<HttpRequest>();
            request->url = "http" + impl_->options.url.substr(2);
            request->ParseUrl();
            for (const auto &[key, value] : impl_->options.headers)
                request->headers.emplace(key, value);
            backend.onConnection = [state, request](const WebSocketChannelPtr &channel) noexcept
            {
                try
                {
                    if (channel->isConnected())
                        state->attach(channel, state->backend->loop())->startClient(*request);
                    else
                    {
                        const auto protocol = channel->getContextPtr<detail::WebSocketProtocol>();
                        const auto code     = protocol        ? protocol->error() :
                                channel->error() == ETIMEDOUT ? NetworkError::ConnectTimeout :
                                                                NetworkError::ConnectFailed;
                        state->closed(channel, code);
                    }
                } catch (...)
                {
                    state->closeError(0, channel, NetworkError::Internal);
                }
            };
            backend.onMessage = WebSocketState::receiveBytes;
            if (backend.createsocket(request->port, request->host.c_str()) < 0)
                return failure(NetworkError::ConnectFailed);
            impl_->active = true;
            bool started  = false;
            defer
            {
                if (!started)
                {
                    impl_->active = false;
                    backend.stop();
                    impl_->clear();
                }
            };
            backend.start();
            started = true;
            return Void::Success();
        });
}
auto WebSocketServer::stop() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inWebSocketCallback)
                return failure(NetworkError::Internal);
            {
                std::lock_guard lock(impl_->gate);
                impl_->active = false;
            }
            if (impl_->backend)
                impl_->backend->stop();
            impl_->clear();
            impl_->backend.reset();
            impl_->port = 0;
            return Void::Success();
        });
}
auto WebSocketClient::stop() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inWebSocketCallback)
                return failure(NetworkError::Internal);
            {
                std::lock_guard lock(impl_->gate);
                impl_->active = false;
            }
            if (impl_->backend)
                impl_->backend->stop();
            impl_->clear();
            impl_->backend.reset();
            return Void::Success();
        });
}
auto WebSocketServer::running() const noexcept -> bool
{
    return impl_->active;
}
auto WebSocketServer::port() const noexcept -> std::uint16_t
{
    return impl_->port;
}
auto WebSocketServer::getChannelById(WebSocketConnectionId id) const -> SZX::COMMON::Result<WebSocketHandle>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<WebSocketHandle>
        {
            std::lock_guard lock(impl_->gate);
            if (!impl_->active)
                return failure<WebSocketHandle>(NetworkError::NotInitialized);
            const auto it = impl_->entries.find(id);
            if (it == impl_->entries.end() || it->second->closing)
                return failure<WebSocketHandle>(NetworkError::Closed);
            return SZX::COMMON::Result<WebSocketHandle>::Success(impl_->handle(id));
        });
}
auto WebSocketServer::connectionNum() const -> SZX::COMMON::Result<std::size_t>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::size_t>
        {
            std::lock_guard lock(impl_->gate);
            std::size_t count = 0;
            if (impl_->active)
                for (const auto &[id, e] : impl_->entries)
                    if (!e->closing)
                        ++count;
            return SZX::COMMON::Result<std::size_t>::Success(count);
        });
}
auto WebSocketServer::foreachChannel(WebSocketChannelVisitor &visitor) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::vector<WebSocketHandle> snapshot;
            {
                std::lock_guard lock(impl_->gate);
                if (!impl_->active)
                    return failure(NetworkError::NotInitialized);
                for (const auto &[id, e] : impl_->entries)
                    if (!e->closing)
                        snapshot.push_back(impl_->handle(id));
            }
            CallbackScope scope;
            for (const auto &connection : snapshot)
                visitor.onChannel(connection);
            return Void::Success();
        });
}
auto WebSocketServer::broadcast(std::string data, WebSocketMessageType type) -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (!validType(type))
                return failure(NetworkError::InvalidArgument);
            struct Visitor : WebSocketChannelVisitor
            {
                const std::string &data;
                WebSocketMessageType type;
                Void result = Void::Success();
                Visitor(const std::string &value, WebSocketMessageType kind) :
                    data(value),
                    type(kind)
                {
                }
                void onChannel(const WebSocketHandle &connection) override
                {
                    if (result.ok)
                        result = connection.send(data, type);
                }
            } visitor(data, type);
            auto result = foreachChannel(visitor);
            return result.ok ? std::move(visitor.result) : result;
        });
}
auto WebSocketHandle::isConnected() const noexcept -> bool
{
    try
    {
        auto state = state_.lock();
        if (!state)
            return false;
        std::lock_guard lock(state->gate);
        auto it = state->entries.find(id_);
        return state->active && it != state->entries.end() && !it->second->closing;
    } catch (...)
    {
        return false;
    }
}
auto WebSocketHandle::peeraddr() const -> SZX::COMMON::Result<Endpoint>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<Endpoint>
        {
            auto state = state_.lock();
            if (!state)
                return failure<Endpoint>(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            auto it = state->entries.find(id_);
            if (!state->active || it == state->entries.end())
                return failure<Endpoint>(NetworkError::Closed);
            return SZX::COMMON::Result<Endpoint>::Success(it->second->peer);
        });
}
auto WebSocketHandle::path() const -> SZX::COMMON::Result<std::string>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::string>
        {
            auto state = state_.lock();
            if (!state)
                return failure<std::string>(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            auto it = state->entries.find(id_);
            if (!state->active || it == state->entries.end())
                return failure<std::string>(NetworkError::Closed);
            return SZX::COMMON::Result<std::string>::Success(it->second->path);
        });
}
auto WebSocketHandle::send(std::string data, WebSocketMessageType type) const -> Void
{
    return boundary(
        [&]() -> Void
        {
            auto state = state_.lock();
            if (!state)
                return failure(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            return state->send(id_, std::move(data), type);
        });
}
auto WebSocketHandle::close() const -> Void
{
    return boundary(
        [&]() -> Void
        {
            auto state = state_.lock();
            if (!state)
                return failure(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            return state->close(id_);
        });
}
auto WebSocketHandle::setContext(WebSocketContext context) const -> Void
{
    return boundary(
        [&]() -> Void
        {
            auto state = state_.lock();
            if (!state)
                return failure(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            return state->setContext(id_, context);
        });
}
auto WebSocketHandle::context() const -> SZX::COMMON::Result<WebSocketContext>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<WebSocketContext>
        {
            auto state = state_.lock();
            if (!state)
                return failure<WebSocketContext>(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            return state->context(id_);
        });
}
auto WebSocketClient::isConnected() const noexcept -> bool
{
    try
    {
        std::lock_guard lock(impl_->gate);
        auto it = impl_->entries.find(impl_->current);
        return impl_->active && it != impl_->entries.end() && !it->second->closing;
    } catch (...)
    {
        return false;
    }
}
auto WebSocketClient::send(std::string data, WebSocketMessageType type) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            return impl_->send(impl_->current, std::move(data), type);
        });
}
auto WebSocketClient::close() -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            return impl_->close(impl_->current);
        });
}
auto WebSocketClient::setContext(WebSocketContext context) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            return impl_->setContext(impl_->current, context);
        });
}
auto WebSocketClient::context() const -> SZX::COMMON::Result<WebSocketContext>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<WebSocketContext>
        {
            std::lock_guard lock(impl_->gate);
            return impl_->context(impl_->current);
        });
}
} // namespace SZX::NETWORK::v1
