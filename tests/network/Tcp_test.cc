#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <set>
#include <stdexcept>
#include "BlockQueue.hpp"
#include "SzxNetwork/TcpClient.h"
#include "SzxNetwork/TcpServer.h"
using namespace SZX::NETWORK;
using namespace std::chrono_literals;

namespace
{
template <typename Base>
struct TestCallbacks final : Base
{
    std::function<void(const TcpHandle &)> connection;
    std::function<void(const TcpHandle &, std::string_view)> message;
    std::function<void(const TcpHandle &, NetworkError)> error;
    void onConnection(const TcpHandle &handle) override
    {
        if (connection)
            connection(handle);
    }
    void onMessage(const TcpHandle &handle, std::string_view bytes) override
    {
        if (message)
            message(handle, bytes);
    }
    void onError(const TcpHandle &handle, NetworkError code) override
    {
        if (error)
            error(handle, code);
    }
};
template <>
struct TestCallbacks<TcpClientCallbacks> final : TcpClientCallbacks
{
    std::function<void(bool)> connection;
    std::function<void(std::string_view)> message;
    std::function<void(NetworkError)> error;
    void onConnection(bool connected) override
    {
        if (connection)
            connection(connected);
    }
    void onMessage(std::string_view bytes) override
    {
        if (message)
            message(bytes);
    }
    void onError(NetworkError code) override
    {
        if (error)
            error(code);
    }
};
struct TestVisitor final : TcpChannelVisitor
{
    explicit TestVisitor(std::function<void(const TcpHandle &)> value) :
        visit(std::move(value))
    {
    }
    std::function<void(const TcpHandle &)> visit;
    void onChannel(const TcpHandle &handle) override
    {
        visit(handle);
    }
};
struct Events
{
    SZX::COMMON::BlockingQueue<TcpConnectionId> connections{64};
    SZX::COMMON::BlockingQueue<std::string> messages{64};
    SZX::COMMON::BlockingQueue<NetworkError> errors{64};
    template <typename Callbacks = TcpServerCallbacks>
    auto callbacks() -> std::shared_ptr<TestCallbacks<Callbacks>>
    {
        auto result = std::make_shared<TestCallbacks<Callbacks>>();
        if constexpr (std::is_same_v<Callbacks, TcpClientCallbacks>)
        {
            result->connection = [this](bool ready)
            {
                if (ready)
                    connections.push(1);
            };
            result->message = [this](auto bytes)
            {
                messages.push(std::string(bytes));
            };
            result->error = [this](auto code)
            {
                errors.push(code);
            };
        }
        else
        {
            result->connection = [this](const TcpHandle &handle)
            {
                if (handle.isConnected())
                    connections.push(handle.id());
            };
            result->message = [this](auto, auto bytes)
            {
                messages.push(std::string(bytes));
            };
            result->error = [this](auto, auto code)
            {
                errors.push(code);
            };
        }
        return result;
    }
};
} // namespace

TEST(TcpTest, FramesSplitAndCoalescedWritesWithBuiltinUnpack)
{
    struct Case
    {
        TcpFraming framing;
        std::string wire;
        std::string first;
        std::string second;
    };
    const std::vector<Case> cases{
        {{.mode = TcpFrameMode::FixedLength, .fixedLength = 3}, "abcdef", "abc", "def"},
        {{.mode = TcpFrameMode::Delimiter, .delimiter = "\r\n"}, "abc\r\ndef\r\n", "abc\r\n", "def\r\n"},
        {{.mode = TcpFrameMode::LengthField},
         std::string("\0\3abc\0\3def", 10),
         std::string("\0\3abc", 5),
         std::string("\0\3def", 5)},
        {{.mode = TcpFrameMode::LengthField, .littleEndian = true},
         std::string("\3\0abc\3\0def", 10),
         std::string("\3\0abc", 5),
         std::string("\3\0def", 5)}};
    for (const auto &tc : cases)
    {
        SCOPED_TRACE(static_cast<int>(tc.framing.mode));
        Events serverEvents, clientEvents;
        auto server = TcpServer::Create({.framing = tc.framing}, serverEvents.callbacks());
        ASSERT_TRUE(server.ok);
        ASSERT_TRUE(server.value->start().ok);
        auto client = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}, .framing = tc.framing},
                                        clientEvents.callbacks<TcpClientCallbacks>());
        ASSERT_TRUE(client.ok);
        ASSERT_TRUE(client.value->start().ok);
        const auto id = serverEvents.connections.pop_for(2s);
        ASSERT_TRUE(id);
        ASSERT_TRUE(clientEvents.connections.pop_for(2s));
        EXPECT_TRUE(client.value->send(tc.wire.substr(0, 1)).ok);
        EXPECT_FALSE(serverEvents.messages.pop_for(30ms));
        EXPECT_TRUE(client.value->send(tc.wire.substr(1)).ok);
        EXPECT_EQ(serverEvents.messages.pop_for(2s), tc.first);
        EXPECT_EQ(serverEvents.messages.pop_for(2s), tc.second);
        auto connection = server.value->getChannelById(*id);
        ASSERT_TRUE(connection.ok);
        EXPECT_TRUE(connection.value.send(tc.wire).ok);
        EXPECT_EQ(clientEvents.messages.pop_for(2s), tc.first);
        EXPECT_EQ(clientEvents.messages.pop_for(2s), tc.second);
        EXPECT_TRUE(client.value->stop().ok);
        EXPECT_TRUE(server.value->stop().ok);
    }
}

TEST(TcpTest, RawBinaryRestartAndStaleConnectionId)
{
    Events serverEvents, clientEvents;
    auto server = TcpServer::Create({}, serverEvents.callbacks());
    ASSERT_TRUE(server.ok);
    TcpConnectionId previous = 0;
    for (int run = 0; run < 3; ++run)
    {
        ASSERT_TRUE(server.value->start().ok);
        EXPECT_EQ(server.value->start().error, static_cast<int>(NetworkError::AlreadyRunning));
        auto client =
            TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, clientEvents.callbacks<TcpClientCallbacks>());
        ASSERT_TRUE(client.ok);
        ASSERT_TRUE(client.value->start().ok);
        auto id = serverEvents.connections.pop_for(2s);
        ASSERT_TRUE(id);
        ASSERT_TRUE(clientEvents.connections.pop_for(2s));
        EXPECT_NE(*id, previous);
        if (previous)
            EXPECT_EQ(server.value->getChannelById(previous).error, static_cast<int>(NetworkError::Closed));
        const std::string binary("a\0b", 3);
        EXPECT_TRUE(client.value->send(binary).ok);
        std::string received;
        while (received.size() < binary.size())
        {
            auto bytes = serverEvents.messages.pop_for(2s);
            ASSERT_TRUE(bytes);
            received += *bytes;
        }
        EXPECT_EQ(received, binary);
        auto connection = server.value->getChannelById(*id);
        ASSERT_TRUE(connection.ok);
        EXPECT_TRUE(connection.value.close().ok);
        EXPECT_TRUE(client.value->stop().ok);
        EXPECT_FALSE(client.value->connected());
        EXPECT_TRUE(server.value->stop().ok);
        EXPECT_FALSE(server.value->running());
        EXPECT_EQ(server.value->port(), 0);
        previous = *id;
    }
}

TEST(TcpTest, QueueLimitCallbackFailureAndLifecycleGuard)
{
    Events events, clientEvents;
    TcpServer *raw     = nullptr;
    auto callbacks     = events.callbacks();
    callbacks->message = [&](auto, auto)
    {
        EXPECT_EQ(raw->stop().error, static_cast<int>(NetworkError::Internal));
        throw std::runtime_error("private diagnostic");
    };
    auto server = TcpServer::Create({}, std::move(callbacks));
    ASSERT_TRUE(server.ok);
    raw = server.value.get();
    ASSERT_TRUE(raw->start().ok);
    auto client = TcpClient::Create({.remote = {"127.0.0.1", raw->port()}, .maxPendingBytes = 4},
                                    clientEvents.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    ASSERT_TRUE(clientEvents.connections.pop_for(2s));
    ASSERT_TRUE(events.connections.pop_for(2s));
    EXPECT_EQ(client.value->send("12345").error, static_cast<int>(NetworkError::SendQueueFull));
    EXPECT_TRUE(client.value->send("test").ok);
    EXPECT_EQ(events.errors.pop_for(2s), NetworkError::Internal);
}

TEST(TcpTest, RejectsInvalidConfigurationAndReportsConnectionFailure)
{
    EXPECT_FALSE(TcpServer::Create({.framing = {.mode = TcpFrameMode::FixedLength}}).ok);
    EXPECT_FALSE(TcpServer::Create({.framing = {.mode = TcpFrameMode::Delimiter, .delimiter = ""}}).ok);
    EXPECT_FALSE(TcpServer::Create({.framing = {.mode = TcpFrameMode::LengthField, .lengthFieldBytes = 4}}).ok);
    EXPECT_FALSE(TcpClient::Create({}).ok);
    auto reserve = TcpServer::Create();
    ASSERT_TRUE(reserve.ok);
    ASSERT_TRUE(reserve.value->start().ok);
    const auto port = reserve.value->port();
    ASSERT_TRUE(reserve.value->stop().ok);
    Events events;
    auto client = TcpClient::Create({.remote = {"127.0.0.1", port}}, events.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    EXPECT_EQ(client.value->send("x").error, static_cast<int>(NetworkError::NotInitialized));
    ASSERT_TRUE(client.value->start().ok);
    const auto error = events.errors.pop_for(6s);
    ASSERT_TRUE(error);
    EXPECT_TRUE(*error == NetworkError::ConnectFailed || *error == NetworkError::ConnectTimeout);
    EXPECT_TRUE(client.value->stop().ok);
    EXPECT_TRUE(client.value->stop().ok);
}

TEST(TcpTest, OversizedFrameIsRejectedAndErrorObserverCannotEscape)
{
    Events events, clientEvents;
    auto callbacks   = events.callbacks();
    callbacks->error = [&](auto, auto error)
    {
        events.errors.push(error);
        throw 42;
    };
    auto server = TcpServer::Create({.framing = {.mode = TcpFrameMode::LengthField, .maxFrameBytes = 16}}, callbacks);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto client =
        TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, clientEvents.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    ASSERT_TRUE(events.connections.pop_for(2s));
    ASSERT_TRUE(clientEvents.connections.pop_for(2s));
    EXPECT_TRUE(client.value->send(std::string("\0\x20", 2)).ok);
    EXPECT_EQ(events.errors.pop_for(2s), NetworkError::MessageTooLarge);
    EXPECT_FALSE(events.messages.pop_for(20ms));
    EXPECT_TRUE(server.value->stop().ok);
}

TEST(TcpTest, ClientRestartAndStopWithPendingSends)
{
    Events serverEvents, clientEvents;
    auto callbacks = serverEvents.callbacks();
    callbacks->message = [](auto, auto) {};
    auto server = TcpServer::Create({}, callbacks);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto client =
        TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, clientEvents.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(client.value->start().ok);
        EXPECT_EQ(client.value->start().error, static_cast<int>(NetworkError::AlreadyRunning));
        ASSERT_TRUE(clientEvents.connections.pop_for(2s));
        ASSERT_TRUE(serverEvents.connections.pop_for(2s));
        for (int j = 0; j < 32; ++j)
            (void)client.value->send(std::string(4096, 'x'));
        EXPECT_TRUE(client.value->stop().ok);
        EXPECT_FALSE(client.value->connected());
    }
}

TEST(TcpTest, SendFromMessageCallbackAndBindingConflict)
{
    Events events;
    TcpServer *raw = nullptr;
    auto echo      = std::make_shared<TestCallbacks<TcpServerCallbacks>>();
    echo->message  = [&](auto id, auto bytes)
    {
        EXPECT_TRUE(id.send(std::string(bytes)).ok);
    };
    auto server = TcpServer::Create({.framing = {.mode = TcpFrameMode::Delimiter}}, echo);
    ASSERT_TRUE(server.ok);
    raw = server.value.get();
    ASSERT_TRUE(raw->start().ok);
    auto conflict = TcpServer::Create({.listen = {"127.0.0.1", raw->port()}});
    ASSERT_TRUE(conflict.ok);
    EXPECT_EQ(conflict.value->start().error, static_cast<int>(NetworkError::BindFailed));
    auto client = TcpClient::Create({.remote = {"127.0.0.1", raw->port()}, .framing = {.mode = TcpFrameMode::Delimiter}},
                                    events.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    ASSERT_TRUE(events.connections.pop_for(2s));
    EXPECT_TRUE(client.value->send("echo\n").ok);
    EXPECT_EQ(events.messages.pop_for(2s), "echo\n");
}

TEST(TcpTest, ContextBroadcastAndForEach)
{
    Events serverEvents, firstEvents, secondEvents;
    auto server = TcpServer::Create({.framing = {.mode = TcpFrameMode::Delimiter}}, serverEvents.callbacks());
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto first  = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}, .framing = {.mode = TcpFrameMode::Delimiter}},
                                    firstEvents.callbacks<TcpClientCallbacks>());
    auto second = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}, .framing = {.mode = TcpFrameMode::Delimiter}},
                                    secondEvents.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(first.ok && second.ok);
    ASSERT_TRUE(first.value->start().ok && second.value->start().ok);
    const auto id1 = serverEvents.connections.pop_for(2s);
    const auto id2 = serverEvents.connections.pop_for(2s);
    ASSERT_TRUE(id1 && id2 && *id1 != *id2);
    auto marker     = std::make_shared<int>(42);
    auto connection = server.value->getChannelById(*id1);
    ASSERT_TRUE(connection.ok);
    ASSERT_TRUE(connection.value.setContext(marker).ok);
    auto saved = connection.value.context();
    ASSERT_TRUE(saved.ok);
    EXPECT_EQ(*std::static_pointer_cast<int>(saved.value), 42);
    std::set<TcpConnectionId> visited;
    TestVisitor collect([&](auto id) { visited.insert(id.id()); });
    ASSERT_TRUE(server.value->forEach(collect).ok);
    EXPECT_EQ(visited, (std::set<TcpConnectionId>{*id1, *id2}));
    ASSERT_TRUE(server.value->broadcast("all\n").ok);
    EXPECT_EQ(firstEvents.messages.pop_for(2s), "all\n");
    EXPECT_EQ(secondEvents.messages.pop_for(2s), "all\n");
    EXPECT_TRUE(first.value->stop().ok && second.value->stop().ok && server.value->stop().ok);
}

TEST(TcpTest, HandlesOwnNoEndpointAndContextSurvivesDisconnectNotification)
{
    static_assert(!std::is_same_v<TcpServerCallbacks, TcpClientCallbacks>);
    static_assert(std::is_abstract_v<TcpServerCallbacks> && std::is_abstract_v<TcpClientCallbacks> &&
                  std::is_abstract_v<TcpChannelVisitor>);
    SZX::COMMON::BlockingQueue<TcpHandle> handles{8};
    SZX::COMMON::BlockingQueue<int> closed{8};
    auto callbacks        = std::make_shared<TestCallbacks<TcpServerCallbacks>>();
    callbacks->connection = [&](const TcpHandle &handle)
    {
        if (handle.isConnected())
        {
            EXPECT_TRUE(handle.newContextPtr<int>(42).ok);
            handles.push(handle);
        }
        else
        {
            auto context = handle.context();
            closed.push(context.ok && context.value ? *std::static_pointer_cast<int>(context.value) : -1);
        }
    };
    auto server = TcpServer::Create({}, callbacks);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    Events events;
    auto client = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, events.callbacks<TcpClientCallbacks>());
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    auto handle = handles.pop_for(2s);
    ASSERT_TRUE(handle);
    ASSERT_TRUE(events.connections.pop_for(2s));
    EXPECT_TRUE(handle->isConnected());
    const auto peer = handle->peeraddr();
    ASSERT_TRUE(peer.ok);
    EXPECT_EQ(peer.value.host, "127.0.0.1");
    EXPECT_NE(peer.value.port, 0);
    auto count = server.value->connectionNum();
    ASSERT_TRUE(count.ok);
    EXPECT_EQ(count.value, 1);
    auto found = server.value->getChannelById(handle->id());
    ASSERT_TRUE(found.ok);
    EXPECT_EQ(found.value.id(), handle->id());
    EXPECT_EQ(server.value->getChannelById(0).error, static_cast<int>(NetworkError::Closed));
    EXPECT_TRUE(client.value->isConnected());
    auto context = handle->getContextPtr<int>();
    ASSERT_TRUE(context.ok && context.value);
    EXPECT_EQ(*context.value, 42);
    struct ThrowingContext
    {
        ThrowingContext()
        {
            throw std::runtime_error("hidden");
        }
    };
    EXPECT_EQ(handle->newContextPtr<ThrowingContext>().error, static_cast<int>(NetworkError::Internal));
    EXPECT_EQ(*handle->getContextPtr<int>().value, 42);
    EXPECT_TRUE(client.value->setContext(std::make_shared<int>(9)).ok);
    EXPECT_TRUE(client.value->setContext({}).ok);
    auto cleared = client.value->context();
    EXPECT_TRUE(cleared.ok && !cleared.value);
    TestVisitor send([](const TcpHandle &peer) { EXPECT_TRUE(peer.write("hello").ok); });
    ASSERT_TRUE(server.value->foreachChannel(send).ok);
    EXPECT_EQ(events.messages.pop_for(2s), "hello");
    TestVisitor throwing([](const TcpHandle &) { throw 42; });
    EXPECT_EQ(server.value->forEach(throwing).error, static_cast<int>(NetworkError::Internal));
    EXPECT_TRUE(handle->close().ok);
    EXPECT_EQ(closed.pop_for(2s), 42);
    ASSERT_TRUE(server.value->stop().ok);
    server.value.reset();
    EXPECT_FALSE(handle->isConnected());
    EXPECT_EQ(handle->peeraddr().error, static_cast<int>(NetworkError::Closed));
    EXPECT_EQ(handle->send("late").error, static_cast<int>(NetworkError::Closed));
    EXPECT_EQ(handle->context().error, static_cast<int>(NetworkError::Closed));
    EXPECT_EQ(handle->disconnect().error, static_cast<int>(NetworkError::Closed));
    EXPECT_EQ(handle->setContext({}).error, static_cast<int>(NetworkError::Closed));
}

TEST(TcpTest, ClientVirtualCallbacksOperateDirectlyOnClientAndAreOwned)
{
    auto echo     = std::make_shared<TestCallbacks<TcpServerCallbacks>>();
    echo->message = [](const TcpHandle &connection, std::string_view bytes)
    {
        EXPECT_TRUE(connection.write(std::string(bytes)).ok);
    };
    auto server = TcpServer::Create({.framing = {.mode = TcpFrameMode::Delimiter}}, echo);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    Events events;
    SZX::COMMON::BlockingQueue<bool> closed{4};
    TcpClient *endpoint   = nullptr;
    auto callbacks        = events.callbacks<TcpClientCallbacks>();
    callbacks->connection = [&](bool ready)
    {
        if (ready)
        {
            EXPECT_TRUE(endpoint->setContext(std::make_shared<int>(7)).ok);
            EXPECT_TRUE(endpoint->send("client callback\n").ok);
        }
        else
        {
            closed.push(!endpoint->isConnected());
        }
    };
    callbacks->message = [&](std::string_view bytes)
    {
        auto context = endpoint->context();
        EXPECT_TRUE(context.ok && context.value);
        EXPECT_TRUE(endpoint->disconnect().ok);
        events.messages.push(std::string(bytes));
    };
    std::weak_ptr<TcpClientCallbacks> lifetime = callbacks;
    auto client = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}, .framing = {.mode = TcpFrameMode::Delimiter}},
                                    callbacks);
    ASSERT_TRUE(client.ok);
    endpoint = client.value.get();
    callbacks.reset();
    EXPECT_FALSE(lifetime.expired());
    ASSERT_TRUE(endpoint->start().ok);
    EXPECT_EQ(events.messages.pop_for(2s), "client callback\n");
    EXPECT_EQ(closed.pop_for(2s), true);
    ASSERT_TRUE(endpoint->stop().ok);
    EXPECT_FALSE(lifetime.expired());
    client.value.reset();
    EXPECT_TRUE(lifetime.expired());
}


TEST(TcpTest, ClosingOrThrowingDropsRemainingFramesInTheSameRead)
{
    for (bool receiverIsClient : {false, true})
    {
        for (bool throws : {false, true})
        {
            SCOPED_TRACE(receiverIsClient ? "client" : "server");
            SCOPED_TRACE(throws ? "exception" : "close");
            std::atomic<int> messages{};
            SZX::COMMON::BlockingQueue<bool> closed{4};
            Events serverEvents, clientEvents;
            auto serverCallbacks = serverEvents.callbacks();
            auto clientCallbacks = clientEvents.callbacks<TcpClientCallbacks>();
            TcpClient *endpoint  = nullptr;
            if (receiverIsClient)
            {
                clientCallbacks->message = [&](std::string_view)
                {
                    ++messages;
                    if (throws)
                        throw std::runtime_error("callback failure");
                    EXPECT_TRUE(endpoint->disconnect().ok);
                };
                clientCallbacks->connection = [&](bool connected)
                {
                    if (connected)
                        clientEvents.connections.push(1);
                    else
                        closed.push(true);
                };
            }
            else
            {
                serverCallbacks->message = [&](const TcpHandle &connection, std::string_view)
                {
                    ++messages;
                    if (throws)
                        throw std::runtime_error("callback failure");
                    EXPECT_TRUE(connection.close().ok);
                };
                serverCallbacks->connection = [&](const TcpHandle &connection)
                {
                    if (connection.isConnected())
                        serverEvents.connections.push(connection.id());
                    else
                        closed.push(true);
                };
            }
            auto server = TcpServer::Create({.framing = {.mode = TcpFrameMode::Delimiter}}, serverCallbacks);
            ASSERT_TRUE(server.ok);
            ASSERT_TRUE(server.value->start().ok);
            auto client = TcpClient::Create(
                {.remote = {"127.0.0.1", server.value->port()}, .framing = {.mode = TcpFrameMode::Delimiter}}, clientCallbacks);
            ASSERT_TRUE(client.ok);
            endpoint = client.value.get();
            ASSERT_TRUE(endpoint->start().ok);
            auto id = serverEvents.connections.pop_for(2s);
            ASSERT_TRUE(id);
            ASSERT_TRUE(clientEvents.connections.pop_for(2s));
            if (receiverIsClient)
            {
                auto connection = server.value->getChannelById(*id);
                ASSERT_TRUE(connection.ok);
                ASSERT_TRUE(connection.value.send("one\ntwo\nthree\n").ok);
            }
            else
                ASSERT_TRUE(endpoint->send("one\ntwo\nthree\n").ok);
            ASSERT_TRUE(closed.pop_for(2s));
            ASSERT_TRUE(endpoint->stop().ok);
            ASSERT_TRUE(server.value->stop().ok);
            EXPECT_EQ(messages.load(), 1);
        }
    }
}

TEST(TcpTest, ClientContextIsReleasedAfterDisconnectWithoutStoppingClient)
{
    for (bool throwOnDisconnect : {false, true})
    {
        SCOPED_TRACE(throwOnDisconnect);
        SZX::COMMON::BlockingQueue<bool> released{4};
        struct Context
        {
            Context(TcpClient &owner, SZX::COMMON::BlockingQueue<bool> &done) :
                client(owner),
                released(done)
            {
            }
            TcpClient &client;
            SZX::COMMON::BlockingQueue<bool> &released;
            ~Context()
            {
                released.push(!client.connected());
            } // Reenters the connection-table lock.
        };
        Events serverEvents, clientEvents;
        auto callbacks        = clientEvents.callbacks<TcpClientCallbacks>();
        callbacks->connection = [&](bool connected)
        {
            if (connected)
                clientEvents.connections.push(1);
            else if (throwOnDisconnect)
                throw 42;
        };
        auto server = TcpServer::Create({}, serverEvents.callbacks());
        ASSERT_TRUE(server.ok);
        ASSERT_TRUE(server.value->start().ok);
        auto client = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, callbacks);
        ASSERT_TRUE(client.ok);
        ASSERT_TRUE(client.value->start().ok);
        ASSERT_TRUE(clientEvents.connections.pop_for(2s));
        auto id = serverEvents.connections.pop_for(2s);
        ASSERT_TRUE(id);
        ASSERT_TRUE(client.value->setContext(std::make_shared<Context>(*client.value, released)).ok);
        auto connection = server.value->getChannelById(*id);
        ASSERT_TRUE(connection.ok);
        ASSERT_TRUE(connection.value.close().ok);
        // Must happen before stop/destruction, even if the disconnect observer throws.
        EXPECT_EQ(released.pop_for(2s), true);
        EXPECT_EQ(client.value->context().error, static_cast<int>(NetworkError::Closed));
        ASSERT_TRUE(client.value->stop().ok);
    }
}
