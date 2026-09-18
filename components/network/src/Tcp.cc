#include <atomic>
#include <climits>
#include <hv/TcpClient.h>
#include <hv/TcpServer.h>
#include <hv/herr.h>
#include <map>
#include <mutex>
#include <variant>
#include <vector>
#include "Defer.hpp"
#include "SzxNetwork/TcpClient.h"
#include "SzxNetwork/TcpServer.h"

namespace SZX::NETWORK::v1
{
namespace
{
using Void                      = SZX::COMMON::Result<void>;
thread_local bool inTcpCallback = false;
template <typename T = void>
auto fail(NetworkError error) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(error), {});
}
template <typename Fn>
auto boundary(Fn &&fn) -> decltype(fn())
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
auto valid(const TcpFraming &f, std::uint32_t pending) -> bool
{
    if (!f.maxFrameBytes || f.maxFrameBytes > INT_MAX || !pending || pending > INT_MAX)
        return false;
    switch (f.mode)
    {
    case TcpFrameMode::Raw:
        return true;
    case TcpFrameMode::FixedLength:
        return f.fixedLength && f.fixedLength <= f.maxFrameBytes;
    case TcpFrameMode::Delimiter:
        return !f.delimiter.empty() && f.delimiter.size() <= 8 && f.delimiter.size() <= f.maxFrameBytes;
    case TcpFrameMode::LengthField:
        return f.lengthFieldBytes >= 1 && f.lengthFieldBytes <= 3 &&
            unsigned(f.lengthFieldOffset) + f.lengthFieldBytes <= f.bodyOffset && f.bodyOffset <= f.maxFrameBytes;
    }
    return false;
}
auto unpack(const TcpFraming &f) -> unpack_setting_t
{
    unpack_setting_t result;
    result.package_max_length = f.maxFrameBytes;
    switch (f.mode)
    {
    case TcpFrameMode::Raw:
        result.mode = UNPACK_MODE_NONE;
        break;
    case TcpFrameMode::FixedLength:
        result.mode         = UNPACK_BY_FIXED_LENGTH;
        result.fixed_length = f.fixedLength;
        break;
    case TcpFrameMode::Delimiter:
        result.mode            = UNPACK_BY_DELIMITER;
        result.delimiter_bytes = static_cast<unsigned short>(f.delimiter.size());
        std::copy(f.delimiter.begin(), f.delimiter.end(), result.delimiter);
        break;
    case TcpFrameMode::LengthField:
        result.mode                = UNPACK_BY_LENGTH_FIELD;
        result.body_offset         = f.bodyOffset;
        result.length_field_offset = f.lengthFieldOffset;
        result.length_field_bytes  = f.lengthFieldBytes;
        result.length_adjustment   = 0;
        result.length_field_coding = f.littleEndian ? ENCODE_BY_LITTEL_ENDIAN : ENCODE_BY_BIG_ENDIAN;
        break;
    }
    return result;
}
struct Entry
{
    hv::SocketChannelPtr channel;
    Endpoint peer;
    TcpConnectionContext context;
    std::atomic_size_t pending{};
    bool closing = false;
};
} // namespace

struct TcpState : std::enable_shared_from_this<TcpState>
{
    using Callbacks = std::variant<std::shared_ptr<TcpServerCallbacks>, std::shared_ptr<TcpClientCallbacks>>;
    explicit TcpState(TcpFraming frame, std::uint32_t limit, Callbacks handlers) :
        framing(std::move(frame)),
        maxPending(limit),
        callbacks(std::move(handlers)),
        setting(unpack(framing))
    {
    }
    TcpFraming framing;
    std::uint32_t maxPending;
    Callbacks callbacks;
    template <typename Fn>
    void notify(Fn &&fn)
    {
        std::visit(
            [&](const auto &handler)
            {
                if (handler)
                    fn(*handler);
            },
            callbacks);
    }
    hv::EventLoopPtr eventLoop;
    auto handle(TcpConnectionId id) -> TcpHandle
    {
        return TcpHandle{weak_from_this(), id};
    }
    unpack_setting_t setting;
    std::mutex gate;
    std::map<TcpConnectionId, std::shared_ptr<Entry>> entries;
    std::atomic_bool active{};
    std::atomic<TcpConnectionId> current{};
    TcpConnectionId nextId = 1;

    void report(TcpConnectionId id, NetworkError error) noexcept
    {
        const auto previous = inTcpCallback;
        inTcpCallback       = true;
        defer
        {
            inTcpCallback = previous;
        };
        try
        {
            notify(
                [&](auto &handler)
                {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(handler)>, TcpServerCallbacks>)
                        handler.onError(handle(id), error);
                    else
                        handler.onError(error);
                });
        } catch (...)
        {
        }
    }
    auto find(const hv::SocketChannelPtr &channel) -> TcpConnectionId
    {
        for (const auto &[id, entry] : entries)
            if (entry->channel == channel)
                return id;
        return 0;
    }
    void closeWithError(TcpConnectionId id, const hv::SocketChannelPtr &channel, NetworkError error)
    {
        {
            std::lock_guard lock(gate);
            const auto it = entries.find(id);
            if (it != entries.end())
                it->second->closing = true;
        }
        report(id, error);
        channel->close(true);
    }
    template <typename Backend>
    void wire(Backend &backend, bool client)
    {
        backend.setUnpack(framing.mode == TcpFrameMode::Raw ? nullptr : &setting);
        backend.onConnection = [this, client](const hv::SocketChannelPtr &incoming) noexcept
        {
            const auto channel  = incoming;
            const auto previous = inTcpCallback;
            inTcpCallback       = true;
            defer
            {
                inTcpCallback = previous;
            };
            TcpConnectionId id = 0;
            std::shared_ptr<Entry> retired;
            defer
            {
                if (retired)
                {
                    {
                        std::lock_guard lock(gate);
                        entries.erase(id);
                    }
                    // The client backend retains its closed channel. Release its
                    // context reference after notification; retired keeps the
                    // object alive until cleanup outside the connection-table lock.
                    channel->setContextPtr(TcpConnectionContext{});
                }
            };
            try
            {
                char ip[64]{};
                auto *address = reinterpret_cast<sockaddr_u *>(hio_peeraddr(channel->io()));
                sockaddr_ip(address, ip, sizeof(ip));
                const Endpoint peer{ip, sockaddr_port(address)};
                const bool connected = channel->isConnected();
                {
                    std::lock_guard lock(gate);
                    if (connected)
                    {
                        id             = nextId++;
                        auto entry     = std::make_shared<Entry>();
                        entry->channel = channel;
                        entry->peer    = peer;
                        entries.emplace(id, std::move(entry));
                        current = id;
                        channel->setMaxWriteBufsize(maxPending);
                        channel->setMaxReadBufsize(framing.maxFrameBytes);
                    }
                    else
                    {
                        id = find(channel);
                        if (id)
                        {
                            retired          = entries.at(id);
                            retired->closing = true;
                        }
                        if (current == id)
                            current = 0;
                    }
                }
                if (!connected && (channel->error() || (client && !id)))
                {
                    const auto error = channel->error();
                    report(id,
                           error == ERR_OVER_LIMIT ? NetworkError::MessageTooLarge :
                               client && !id ? (error == ETIMEDOUT ? NetworkError::ConnectTimeout : NetworkError::ConnectFailed) :
                                               NetworkError::Closed);
                }
                notify(
                    [&](auto &handler)
                    {
                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(handler)>, TcpServerCallbacks>)
                            handler.onConnection(handle(id));
                        else
                            handler.onConnection(connected);
                    });
            } catch (...)
            {
                closeWithError(id, channel, NetworkError::Internal);
            }
        };
        backend.onMessage = [this](const hv::SocketChannelPtr &incoming, hv::Buffer *buffer) noexcept
        {
            const auto channel  = incoming;
            const auto previous = inTcpCallback;
            inTcpCallback       = true;
            defer
            {
                inTcpCallback = previous;
            };
            TcpConnectionId id = 0;
            try
            {
                {
                    std::lock_guard lock(gate);
                    id = find(channel);
                    if (!active || !id || entries.at(id)->closing)
                        return;
                }
                if (buffer->size() > framing.maxFrameBytes)
                {
                    closeWithError(id, channel, NetworkError::MessageTooLarge);
                    return;
                }
                notify(
                    [&](auto &handler)
                    {
                        const std::string_view bytes{static_cast<const char *>(buffer->data()), buffer->size()};
                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(handler)>, TcpServerCallbacks>)
                            handler.onMessage(handle(id), bytes);
                        else
                            handler.onMessage(bytes);
                    });
            } catch (...)
            {
                closeWithError(id, channel, NetworkError::Internal);
            }
        };
        backend.onWriteComplete = [this](const hv::SocketChannelPtr &channel, hv::Buffer *buffer) noexcept
        {
            try
            {
                std::lock_guard lock(gate);
                const auto id = find(channel);
                if (id)
                    entries.at(id)->pending.fetch_sub(buffer->size());
            } catch (...)
            {
                report(0, NetworkError::Internal);
            }
        };
    }
    auto send(TcpConnectionId id, std::string data, const hv::EventLoopPtr &loop) -> Void
    {
        if (!active)
            return fail(NetworkError::NotInitialized);
        const auto it = entries.find(id);
        if (it == entries.end() || it->second->closing)
            return fail(NetworkError::Closed);
        if (data.empty())
            return Void::Success();
        const auto entry = it->second;
        if (data.size() > maxPending || entry->pending.load() > maxPending - data.size())
            return fail(NetworkError::SendQueueFull);
        const auto size  = data.size();
        entry->pending  += size;
        bool queued      = false;
        defer
        {
            if (!queued)
                entry->pending -= size;
        };
        loop->queueInLoop(
            [this, id, weak = std::weak_ptr<Entry>(entry), data = std::move(data)]() noexcept
            {
                const auto entry = weak.lock();
                if (!entry)
                    return;
                try
                {
                    if (!entry->channel->isConnected())
                    {
                        entry->pending -= data.size();
                        report(id, NetworkError::Closed);
                        return;
                    }
                    if (entry->channel->write(data.data(), static_cast<int>(data.size())) < 0)
                    {
                        closeWithError(id, entry->channel, NetworkError::SendFailed);
                    }
                } catch (...)
                {
                    closeWithError(id, entry->channel, NetworkError::Internal);
                }
            });
        queued = true;
        return Void::Success();
    }
    auto disconnect(TcpConnectionId id, const hv::EventLoopPtr &loop) -> Void
    {
        if (!active)
            return fail(NetworkError::NotInitialized);
        const auto it = entries.find(id);
        if (it == entries.end())
            return fail(NetworkError::Closed);
        const auto entry = it->second;
        if (!entry->closing)
        {
            loop->queueInLoop(
                [weak = std::weak_ptr<Entry>(entry)]() noexcept
                {
                    if (const auto entry = weak.lock())
                        entry->channel->close(true);
                });
            entry->closing = true;
        }
        return Void::Success();
    }
    auto setContext(TcpConnectionId id, TcpConnectionContext &context) -> Void
    {
        if (!active)
            return fail(NetworkError::NotInitialized);
        const auto it = entries.find(id);
        if (it == entries.end())
            return fail(NetworkError::Closed);
        it->second->context.swap(context);
        it->second->channel->setContextPtr(it->second->context);
        return Void::Success();
    }
    auto context(TcpConnectionId id) const -> SZX::COMMON::Result<TcpConnectionContext>
    {
        if (!active)
            return fail<TcpConnectionContext>(NetworkError::NotInitialized);
        const auto it = entries.find(id);
        if (it == entries.end())
            return fail<TcpConnectionContext>(NetworkError::Closed);
        return SZX::COMMON::Result<TcpConnectionContext>::Success(it->second->context);
    }
    auto ids() const -> std::vector<TcpConnectionId>
    {
        std::vector<TcpConnectionId> result;
        result.reserve(entries.size());
        for (const auto &[id, entry] : entries)
            if (!entry->closing)
                result.push_back(id);
        return result;
    }
    template <typename Backend>
    void stop(std::unique_ptr<Backend> &backend)
    {
        {
            std::lock_guard lock(gate);
            active = false;
        }
        if (!backend)
            return;
        backend->stop(true);
        backend->onConnection    = {};
        backend->onMessage       = {};
        backend->onWriteComplete = {};
        decltype(entries) retired;
        {
            std::lock_guard lock(gate);
            retired.swap(entries);
            current = 0;
        }
        backend.reset();
    }
};

auto TcpHandle::isConnected() const noexcept -> bool
{
    try
    {
        const auto state = state_.lock();
        if (!state)
            return false;
        std::lock_guard lock(state->gate);
        const auto it = state->entries.find(id_);
        return state->active && it != state->entries.end() && !it->second->closing;
    } catch (...)
    {
        return false;
    }
}
auto TcpHandle::peeraddr() const -> SZX::COMMON::Result<Endpoint>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<Endpoint>
        {
            const auto state = state_.lock();
            if (!state)
                return fail<Endpoint>(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            const auto it = state->entries.find(id_);
            if (!state->active || it == state->entries.end())
                return fail<Endpoint>(NetworkError::Closed);
            return SZX::COMMON::Result<Endpoint>::Success(it->second->peer);
        });
}
auto TcpHandle::send(std::string data) const -> Void
{
    return boundary(
        [&]() -> Void
        {
            const auto state = state_.lock();
            if (!state)
                return fail(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            if (!state->active)
                return fail(NetworkError::Closed);
            return state->send(id_, std::move(data), state->eventLoop);
        });
}
auto TcpHandle::disconnect() const -> Void
{
    return boundary(
        [&]() -> Void
        {
            const auto state = state_.lock();
            if (!state)
                return fail(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            if (!state->active)
                return fail(NetworkError::Closed);
            return state->disconnect(id_, state->eventLoop);
        });
}
auto TcpHandle::setContext(TcpConnectionContext context) const -> Void
{
    return boundary(
        [&]() -> Void
        {
            const auto state = state_.lock();
            if (!state)
                return fail(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            if (!state->active)
                return fail(NetworkError::Closed);
            return state->setContext(id_, context);
        });
}
auto TcpHandle::context() const -> SZX::COMMON::Result<TcpConnectionContext>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<TcpConnectionContext>
        {
            const auto state = state_.lock();
            if (!state)
                return fail<TcpConnectionContext>(NetworkError::Closed);
            std::lock_guard lock(state->gate);
            if (!state->active)
                return fail<TcpConnectionContext>(NetworkError::Closed);
            return state->context(id_);
        });
}
class TcpServer::Impl : public TcpState
{
public:
    Impl(TcpServerOptions value, std::shared_ptr<TcpServerCallbacks> callbacks) :
        TcpState(value.framing, value.maxPendingBytes, std::move(callbacks)),
        options(std::move(value))
    {
    }
    TcpServerOptions options;
    std::atomic_uint16_t port{};
    std::unique_ptr<hv::TcpServer> backend;
};

class TcpClient::Impl : public TcpState
{
public:
    Impl(TcpClientOptions value, std::shared_ptr<TcpClientCallbacks> callbacks) :
        TcpState(value.framing, value.maxPendingBytes, std::move(callbacks)),
        options(std::move(value))
    {
    }
    TcpClientOptions options;
    std::unique_ptr<hv::TcpClient> backend;
};
TcpServer::TcpServer(std::shared_ptr<Impl> impl) :
    impl_(std::move(impl))
{
}
TcpClient::TcpClient(std::shared_ptr<Impl> impl) :
    impl_(std::move(impl))
{
}
TcpServer::~TcpServer()
{
    (void)stop();
}
TcpClient::~TcpClient()
{
    (void)stop();
}
auto TcpServer::Create(TcpServerOptions options, std::shared_ptr<TcpServerCallbacks> callbacks)
    -> SZX::COMMON::Result<std::unique_ptr<TcpServer>>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::unique_ptr<TcpServer>>
        {
            if (!valid(options.framing, options.maxPendingBytes) || !options.maxConnections ||
                options.listen.host.find('\0') != std::string::npos)
                return fail<std::unique_ptr<TcpServer>>(NetworkError::InvalidArgument);
            if (options.listen.host.empty())
                options.listen.host = "127.0.0.1";
            return SZX::COMMON::Result<std::unique_ptr<TcpServer>>::Success(
                std::unique_ptr<TcpServer>(new TcpServer(std::make_shared<Impl>(std::move(options), std::move(callbacks)))));
        });
}
auto TcpClient::Create(TcpClientOptions options, std::shared_ptr<TcpClientCallbacks> callbacks)
    -> SZX::COMMON::Result<std::unique_ptr<TcpClient>>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::unique_ptr<TcpClient>>
        {
            if (!valid(options.framing, options.maxPendingBytes) || !options.remote.port || options.remote.host.empty() ||
                options.remote.host.find('\0') != std::string::npos || !options.connectTimeoutMs ||
                options.connectTimeoutMs > INT_MAX)
                return fail<std::unique_ptr<TcpClient>>(NetworkError::InvalidArgument);
            return SZX::COMMON::Result<std::unique_ptr<TcpClient>>::Success(
                std::unique_ptr<TcpClient>(new TcpClient(std::make_shared<Impl>(std::move(options), std::move(callbacks)))));
        });
}
auto TcpServer::start() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inTcpCallback)
                return fail(NetworkError::Internal);
            if (impl_->active)
                return fail(NetworkError::AlreadyRunning);
            impl_->backend = std::make_unique<hv::TcpServer>();
            bool started   = false;
            defer
            {
                if (!started)
                    impl_->stop(impl_->backend);
            };
            auto &backend    = *impl_->backend;
            impl_->eventLoop = backend.loop();
            impl_->wire(backend, false);
            backend.setThreadNum(0); // Only the acceptor IO thread; no implicit hardware-sized worker pool.
            backend.setMaxConnectionNum(impl_->options.maxConnections);
            const auto fd = backend.createsocket(impl_->options.listen.port, impl_->options.listen.host.c_str());
            if (fd < 0)
                return fail(NetworkError::BindFailed);
            if (!hio_get(backend.loop()->loop(), fd))
            {
                ::closesocket(fd);
                backend.listenfd = -1;
                return fail(NetworkError::Internal);
            }
            sockaddr_u address{};
            socklen_t size = sizeof(address);
            if (getsockname(fd, &address.sa, &size) != 0)
                return fail(NetworkError::BindFailed);
            impl_->active = true;
            backend.start(true);
            impl_->port = sockaddr_port(&address);
            started     = true;
            return Void::Success();
        });
}
auto TcpClient::start() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inTcpCallback)
                return fail(NetworkError::Internal);
            if (impl_->active)
                return fail(NetworkError::AlreadyRunning);
            impl_->backend = std::make_unique<hv::TcpClient>();
            bool started   = false;
            defer
            {
                if (!started)
                    impl_->stop(impl_->backend);
            };
            auto &backend    = *impl_->backend;
            impl_->eventLoop = backend.loop();
            impl_->wire(backend, true);
            backend.setConnectTimeout(static_cast<int>(impl_->options.connectTimeoutMs));
            sockaddr_u address{};
            if (sockaddr_set_ipport(&address, impl_->options.remote.host.c_str(), impl_->options.remote.port) != 0)
                return fail(NetworkError::ResolveFailed);
            backend.remote_addr = address;
            if (backend.createsocket(&address.sa) < 0)
                return fail(NetworkError::ConnectFailed);
            impl_->active = true;
            backend.start(true);
            started = true;
            return Void::Success();
        });
}
auto TcpServer::stop() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inTcpCallback)
                return fail(NetworkError::Internal);
            impl_->stop(impl_->backend);
            impl_->port = 0;
            return Void::Success();
        });
}
auto TcpClient::stop() -> Void
{
    return boundary(
        [&]() -> Void
        {
            if (inTcpCallback)
                return fail(NetworkError::Internal);
            impl_->stop(impl_->backend);
            return Void::Success();
        });
}
auto TcpServer::broadcast(std::string data) -> Void
{
    return boundary(
        [&]() -> Void
        {
            struct Broadcast final : TcpChannelVisitor
            {
                explicit Broadcast(const std::string &value) :
                    data(value)
                {
                }
                const std::string &data;
                Void result = Void::Success();
                void onChannel(const TcpHandle &connection) override
                {
                    if (result.ok)
                        result = connection.send(data);
                }
            } visitor(data);
            auto visited = forEach(visitor);
            return visited.ok ? std::move(visitor.result) : std::move(visited);
        });
}
auto TcpServer::forEach(TcpChannelVisitor &visitor) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::vector<TcpConnectionId> snapshot;
            {
                std::lock_guard lock(impl_->gate);
                if (!impl_->active)
                    return fail(NetworkError::NotInitialized);
                snapshot = impl_->ids();
            }
            const auto previous = inTcpCallback;
            inTcpCallback       = true;
            defer
            {
                inTcpCallback = previous;
            };
            for (const auto id : snapshot)
                visitor.onChannel(impl_->handle(id));
            return Void::Success();
        });
}
auto TcpClient::send(std::string data) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            if (!impl_->active)
                return fail(NetworkError::NotInitialized);
            return impl_->send(impl_->current, std::move(data), impl_->backend->loop());
        });
}
auto TcpClient::disconnect() -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            if (!impl_->active)
                return fail(NetworkError::NotInitialized);
            return impl_->disconnect(impl_->current, impl_->backend->loop());
        });
}
auto TcpClient::setContext(TcpConnectionContext context) -> Void
{
    return boundary(
        [&]() -> Void
        {
            std::lock_guard lock(impl_->gate);
            return impl_->setContext(impl_->current, context);
        });
}
auto TcpClient::context() const -> SZX::COMMON::Result<TcpConnectionContext>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<TcpConnectionContext>
        {
            std::lock_guard lock(impl_->gate);
            return impl_->context(impl_->current);
        });
}
auto TcpServer::getChannelById(TcpConnectionId id) const -> SZX::COMMON::Result<TcpHandle>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<TcpHandle>
        {
            std::lock_guard lock(impl_->gate);
            if (!impl_->active)
                return fail<TcpHandle>(NetworkError::NotInitialized);
            const auto it = impl_->entries.find(id);
            if (it == impl_->entries.end() || it->second->closing)
                return fail<TcpHandle>(NetworkError::Closed);
            return SZX::COMMON::Result<TcpHandle>::Success(impl_->handle(id));
        });
}
auto TcpServer::connectionNum() const -> SZX::COMMON::Result<std::size_t>
{
    return boundary(
        [&]() -> SZX::COMMON::Result<std::size_t>
        {
            std::lock_guard lock(impl_->gate);
            std::size_t count = 0;
            if (impl_->active)
                for (const auto &[id, entry] : impl_->entries)
                    if (!entry->closing)
                        ++count;
            return SZX::COMMON::Result<std::size_t>::Success(count);
        });
}
auto TcpServer::running() const noexcept -> bool
{
    return impl_->active;
}
auto TcpServer::port() const noexcept -> std::uint16_t
{
    return impl_->port;
}
auto TcpClient::connected() const noexcept -> bool
{
    try
    {
        std::lock_guard lock(impl_->gate);
        const auto it = impl_->entries.find(impl_->current);
        return impl_->active && it != impl_->entries.end() && !it->second->closing;
    } catch (...)
    {
        return false;
    }
}
} // namespace SZX::NETWORK::v1
