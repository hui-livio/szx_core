
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>
#include "../support/HttpClient.h"
#include "BlockQueue.hpp"
#include "SzxNetwork/HttpClient.h"
#include "SzxNetwork/HttpServer.h"
#include "SzxNetwork/TcpClient.h"
#include "SzxNetwork/TcpServer.h"
#include "SzxNetwork/WebSocketClient.h"
#include "SzxNetwork/WebSocketServer.h"
using namespace SZX::NETWORK;
using namespace std::chrono_literals;
namespace
{
struct Message
{
    std::string bytes;
    WebSocketMessageType type;
};
struct ServerEvents final : WebSocketServerCallbacks
{
    SZX::COMMON::BlockingQueue<WebSocketHandle> opened{16};
    SZX::COMMON::BlockingQueue<bool> closed{16};
    SZX::COMMON::BlockingQueue<Message> messages{16};
    SZX::COMMON::BlockingQueue<NetworkError> errors{16};
    bool echo{}, throws{};
    void onConnection(const WebSocketHandle &connection) override
    {
        if (connection.isConnected())
            opened.push(connection);
        else
            closed.push(true);
    }
    void onMessage(const WebSocketHandle &connection, std::string_view bytes, WebSocketMessageType type) override
    {
        if (throws)
            throw std::runtime_error("internal only");
        if (echo)
            EXPECT_TRUE(connection.send(std::string(bytes), type).ok);
        else
            messages.push({std::string(bytes), type});
    }
    void onError(const WebSocketHandle &, NetworkError code) override
    {
        errors.push(code);
    }
};
struct ClientEvents final : WebSocketClientCallbacks
{
    SZX::COMMON::BlockingQueue<bool> connections{16};
    SZX::COMMON::BlockingQueue<Message> messages{16};
    SZX::COMMON::BlockingQueue<NetworkError> errors{16};
    void onConnection(bool ready) override
    {
        connections.push(ready);
    }
    void onMessage(std::string_view bytes, WebSocketMessageType type) override
    {
        messages.push({std::string(bytes), type});
    }
    void onError(NetworkError code) override
    {
        errors.push(code);
    }
};
std::string url(WebSocketServer &server)
{
    return "ws://127.0.0.1:" + std::to_string(server.port()) + "/echo";
}

const std::string handshake = "GET /chat HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";

struct RawEvents : TcpClientCallbacks
{
    SZX::COMMON::BlockingQueue<bool> connections{16};
    SZX::COMMON::BlockingQueue<std::string> bytes{32};
    void onConnection(bool ready) override
    {
        connections.push(ready);
    }
    void onMessage(std::string_view data) override
    {
        bytes.push(std::string(data));
    }
    void onError(NetworkError) override
    {
    }
};
struct RawClient
{
    std::shared_ptr<RawEvents> events = std::make_shared<RawEvents>();
    std::unique_ptr<TcpClient> client;
    std::string pending;
    explicit RawClient(std::uint16_t port)
    {
        auto created = TcpClient::Create({.remote = {"127.0.0.1", port}}, events);
        if (!created.ok)
            throw std::runtime_error("raw client create failed");
        client = std::move(created.value);
        if (!client->start().ok || events->connections.pop_for(3s) != true)
            throw std::runtime_error("raw client connect failed");
    }
    void send(const std::string &data)
    {
        if (!client->send(data).ok)
            throw std::runtime_error("raw client send failed");
    }
    void readMore()
    {
        auto bytes = events->bytes.pop_for(3s);
        if (!bytes)
            throw std::runtime_error("raw client receive timeout");
        pending += *bytes;
    }
    std::string headers()
    {
        while (pending.find("\r\n\r\n") == pending.npos)
            readMore();
        const auto end = pending.find("\r\n\r\n") + 4;
        auto result    = pending.substr(0, end);
        pending.erase(0, end);
        return result;
    }
    std::string take(std::size_t size)
    {
        while (pending.size() < size)
            readMore();
        auto result = pending.substr(0, size);
        pending.erase(0, size);
        return result;
    }
};

// Independent wire fixtures; no hv framing/parser helpers are linked into these tests.
std::string frameHeader(unsigned char opcode, std::uint64_t length, bool masked)
{
    std::string bytes(1, static_cast<char>(opcode));
    const auto mask = masked ? 0x80 : 0;
    if (length < 126)
        bytes += static_cast<char>(mask | length);
    else
    {
        bytes += static_cast<char>(mask | (length <= 65535 ? 126 : 127));
        for (int shift = length <= 65535 ? 8 : 56; shift >= 0; shift -= 8)
            bytes += static_cast<char>((length >> shift) & 0xff);
    }
    if (masked)
        bytes.append(4, '\0');
    return bytes;
}
std::string frame(unsigned char opcode, std::string_view data, bool masked = true)
{
    return frameHeader(opcode, data.size(), masked) + std::string(data);
}

// Obtain a real handshake response, then act as a raw server to the client under
// test. This avoids coupling the frame regressions to the library's frame sender.
struct RawServerEvents : TcpServerCallbacks
{
    struct Context
    {
        std::string request;
        bool upgraded{};
    };
    std::uint16_t handshakePort{};
    std::function<void(std::string &)> responseEdit;
    std::string initialFrames;
    SZX::COMMON::BlockingQueue<std::pair<TcpHandle, std::string>> requests{16};
    SZX::COMMON::BlockingQueue<TcpHandle> opened{16};
    SZX::COMMON::BlockingQueue<std::string> bytes{32};
    void onConnection(const TcpHandle &connection) override
    {
        if (connection.isConnected())
            EXPECT_TRUE(connection.setContext(std::make_shared<Context>()).ok);
    }
    void onMessage(const TcpHandle &connection, std::string_view data) override
    {
        const auto ctx = connection.getContextPtr<Context>().value;
        if (ctx->upgraded)
        {
            bytes.push(std::string(data));
            return;
        }
        ctx->request += data;
        if (ctx->request.find("\r\n\r\n") == std::string::npos)
            return;
        ctx->upgraded = true;
        requests.push({connection, std::move(ctx->request)});
    }
    void respond()
    {
        auto request = requests.pop_for(3s);
        if (!request)
            throw std::runtime_error("raw server handshake timeout");
        SZX::TEST::HTTP::Client signer(handshakePort);
        signer.send(request->second);
        auto response = signer.read(true);
        if (responseEdit)
            responseEdit(response);
        EXPECT_TRUE(request->first.send(response + initialFrames).ok);
        opened.push(request->first);
    }
    void onError(const TcpHandle &, NetworkError) override
    {
    }
};
} // namespace
TEST(WebSocketTest, TextBinaryEmptyAndFragmentedRoundtrip)
{
    auto events  = std::make_shared<ServerEvents>();
    events->echo = true;
    auto server  = WebSocketServer::Create({}, events);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto received = std::make_shared<ClientEvents>();
    auto client   = WebSocketClient::Create({.url = url(*server.value)}, received);
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    ASSERT_EQ(received->connections.pop_for(3s), true);
    auto connection = events->opened.pop_for(3s);
    ASSERT_TRUE(connection);
    EXPECT_EQ(connection->path().value, "/echo");
    EXPECT_EQ(connection->peeraddr().value.host, "127.0.0.1");
    for (const auto &tc : std::vector<Message>{{"hello", WebSocketMessageType::Text},
                                               {std::string("a\0b", 3), WebSocketMessageType::Binary},
                                               {"", WebSocketMessageType::Text},
                                               {std::string(100000, 'x'), WebSocketMessageType::Binary}})
    {
        ASSERT_TRUE(client.value->send(tc.bytes, tc.type).ok);
        auto reply = received->messages.pop_for(3s);
        ASSERT_TRUE(reply);
        EXPECT_EQ(reply->bytes, tc.bytes);
        EXPECT_EQ(reply->type, tc.type);
    }
    EXPECT_TRUE(client.value->close().ok);
    EXPECT_EQ(received->connections.pop_for(3s), false);
    EXPECT_TRUE(events->closed.pop_for(3s));
    ASSERT_TRUE(client.value->stop().ok);
    ASSERT_TRUE(server.value->stop().ok);
    EXPECT_EQ(connection->send("late").error, static_cast<int>(NetworkError::Closed));
}
TEST(WebSocketTest, MultipleConnectionsBroadcastContextAndRestart)
{
    auto events = std::make_shared<ServerEvents>();
    auto server = WebSocketServer::Create({}, events);
    ASSERT_TRUE(server.ok);
    WebSocketHandle old;
    for (int round = 0; round < 2; ++round)
    {
        ASSERT_TRUE(server.value->start().ok);
        auto a = std::make_shared<ClientEvents>(), b = std::make_shared<ClientEvents>();
        auto ca = WebSocketClient::Create({.url = url(*server.value)}, a),
             cb = WebSocketClient::Create({.url = url(*server.value)}, b);
        ASSERT_TRUE(ca.ok && cb.ok);
        ASSERT_TRUE(ca.value->start().ok && cb.value->start().ok);
        ASSERT_EQ(a->connections.pop_for(3s), true);
        ASSERT_EQ(b->connections.pop_for(3s), true);
        auto first = events->opened.pop_for(3s), second = events->opened.pop_for(3s);
        ASSERT_TRUE(first && second);
        EXPECT_NE(first->id(), old.id());
        EXPECT_FALSE(old.isConnected());
        EXPECT_EQ(server.value->connectionNum().value, 2);
        EXPECT_TRUE(first->setContext(std::make_shared<int>(42)).ok);
        auto ctx = first->getContextPtr<int>();
        ASSERT_TRUE(ctx.ok && ctx.value);
        EXPECT_EQ(*ctx.value, 42);
        EXPECT_TRUE(ca.value->setContext(std::make_shared<int>(7)).ok);
        EXPECT_TRUE(ca.value->context().ok);
        struct Visitor : WebSocketChannelVisitor
        {
            int count{};
            void onChannel(const WebSocketHandle &c) override
            {
                ++count;
                EXPECT_TRUE(c.send("visit").ok);
            }
        } visitor;
        EXPECT_TRUE(server.value->foreachChannel(visitor).ok);
        EXPECT_EQ(visitor.count, 2);
        ASSERT_TRUE(a->messages.pop_for(3s));
        ASSERT_TRUE(b->messages.pop_for(3s));
        ASSERT_TRUE(server.value->broadcast("all").ok);
        auto messageA = a->messages.pop_for(3s);
        auto messageB = b->messages.pop_for(3s);
        ASSERT_TRUE(messageA && messageB);
        EXPECT_EQ(messageA->bytes, "all");
        EXPECT_EQ(messageB->bytes, "all");
        old = *first;
        EXPECT_TRUE(ca.value->stop().ok && cb.value->stop().ok);
        EXPECT_TRUE(server.value->stop().ok);
    }
}
TEST(WebSocketTest, ValidationQueueLimitsAndCallbackException)
{
    EXPECT_FALSE(WebSocketClient::Create({.url = "http://example.com"}).ok);
    EXPECT_EQ(WebSocketClient::Create({.url = "wss://localhost"}).error, static_cast<int>(NetworkError::TlsUnavailable));
    EXPECT_FALSE(WebSocketClient::Create({.url = "ws://localhost/", .headers = {{"Connection", "bad"}}}).ok);
    auto se     = std::make_shared<ServerEvents>();
    se->throws  = true;
    auto server = WebSocketServer::Create({}, se);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto ce     = std::make_shared<ClientEvents>();
    auto client = WebSocketClient::Create({.url = url(*server.value), .maxMessageBytes = 256, .maxPendingBytes = 128}, ce);
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    ASSERT_EQ(ce->connections.pop_for(3s), true);
    EXPECT_EQ(client.value->send(std::string(257, 'x')).error, static_cast<int>(NetworkError::MessageTooLarge));
    EXPECT_EQ(client.value->send(std::string(128, 'x')).error, static_cast<int>(NetworkError::SendQueueFull));
    ASSERT_TRUE(client.value->send("fail").ok);
    EXPECT_EQ(se->errors.pop_for(3s), NetworkError::Internal);
    EXPECT_EQ(ce->connections.pop_for(3s), false);
    EXPECT_TRUE(client.value->stop().ok && server.value->stop().ok);
}
TEST(WebSocketTest, OversizedIncomingMessageAndClientRestart)
{
    auto se     = std::make_shared<ServerEvents>();
    auto server = WebSocketServer::Create({.maxMessageBytes = 16}, se);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto ce     = std::make_shared<ClientEvents>();
    auto client = WebSocketClient::Create({.url = url(*server.value)}, ce);
    ASSERT_TRUE(client.ok);
    for (int run = 0; run < 2; ++run)
    {
        ASSERT_TRUE(client.value->start().ok);
        ASSERT_EQ(ce->connections.pop_for(3s), true);
        ASSERT_TRUE(client.value->send(std::string(32, 'a')).ok);
        EXPECT_EQ(se->errors.pop_for(3s), NetworkError::MessageTooLarge);
        EXPECT_EQ(ce->connections.pop_for(3s), false);
        ASSERT_TRUE(client.value->stop().ok);
    }
    ASSERT_TRUE(server.value->stop().ok);
}

TEST(WebSocketTest, RejectsFailedUpgradeAndReportsConnectionFailure)
{
    auto httpServer = HttpServer::Create({});
    ASSERT_TRUE(httpServer.ok);
    ASSERT_TRUE(
        httpServer.value->get("/", [](const HttpServerRequest &) { return HttpServerResponse{.body = "not a websocket"}; }).ok);
    ASSERT_TRUE(httpServer.value->start().ok);
    auto events = std::make_shared<ClientEvents>();
    auto client = WebSocketClient::Create({.url = "ws://127.0.0.1:" + std::to_string(httpServer.value->port()) + "/"}, events);
    ASSERT_TRUE(client.ok);
    ASSERT_TRUE(client.value->start().ok);
    EXPECT_EQ(events->errors.pop_for(3s), NetworkError::ProtocolError);
    EXPECT_EQ(events->connections.pop_for(3s), false);
    EXPECT_FALSE(client.value->isConnected());
    ASSERT_TRUE(client.value->stop().ok);
    const auto port = httpServer.value->port();
    ASSERT_TRUE(httpServer.value->stop().ok);
    auto failure = WebSocketClient::Create({.url = "ws://127.0.0.1:" + std::to_string(port) + "/"}, events);
    ASSERT_TRUE(failure.ok);
    ASSERT_TRUE(failure.value->start().ok);
    EXPECT_EQ(events->errors.pop_for(3s), NetworkError::ConnectFailed);
    EXPECT_EQ(events->connections.pop_for(3s), false);
    ASSERT_TRUE(failure.value->stop().ok);
}
TEST(WebSocketTest, RejectsMalformedUrlsAndDefaultHttpIsNotServed)
{
    for (const auto value :
         {"ws://", "ws:///path", "ws://localhost:0/", "ws://localhost:99999/", "ws://localhost/\r\n", "ws://user@localhost/"})
    {
        SCOPED_TRACE(value);
        EXPECT_FALSE(WebSocketClient::Create({.url = value}).ok);
    }
#if SZX_NETWORK_TEST_HTTP_CLIENT
    auto server = WebSocketServer::Create();
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    auto response = http::get("http://127.0.0.1:" + std::to_string(server.value->port()) + "/");
    ASSERT_TRUE(response.ok);
    EXPECT_EQ(response.value.statusCode, 404);
    EXPECT_TRUE(server.value->stop().ok);
#endif
}

TEST(WebSocketTest, RfcHandshakeMaskedTextAndPingUsingRawTcpPeer)
{
    auto events  = std::make_shared<ServerEvents>();
    events->echo = true;
    auto server  = WebSocketServer::Create({}, events);
    ASSERT_TRUE(server.ok);
    ASSERT_TRUE(server.value->start().ok);
    struct RawEvents : TcpClientCallbacks
    {
        SZX::COMMON::BlockingQueue<bool> connections{4};
        SZX::COMMON::BlockingQueue<std::string> bytes{16};
        void onConnection(bool ready) override
        {
            connections.push(ready);
        }
        void onMessage(std::string_view data) override
        {
            bytes.push(std::string(data));
        }
        void onError(NetworkError) override
        {
        }
    };
    auto rawEvents = std::make_shared<RawEvents>();
    auto raw       = TcpClient::Create({.remote = {"127.0.0.1", server.value->port()}}, rawEvents);
    ASSERT_TRUE(raw.ok);
    ASSERT_TRUE(raw.value->start().ok);
    ASSERT_EQ(rawEvents->connections.pop_for(3s), true);
    ASSERT_TRUE(
        raw.value
            ->send(
                "GET /chat HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n")
            .ok);
    std::string response;
    while (response.find("\r\n\r\n") == std::string::npos)
    {
        auto part = rawEvents->bytes.pop_for(3s);
        ASSERT_TRUE(part);
        response += *part;
    }
    EXPECT_TRUE(response.starts_with("HTTP/1.1 101"));
    EXPECT_NE(response.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), std::string::npos);
    ASSERT_TRUE(raw.value->send(std::string("\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11)).ok);
    response.clear();
    while (response.size() < 7)
    {
        auto part = rawEvents->bytes.pop_for(3s);
        ASSERT_TRUE(part);
        response += *part;
    }
    EXPECT_EQ(response, std::string("\x81\x05Hello", 7));
    ASSERT_TRUE(raw.value->send(std::string("\x89\x80\0\0\0\0", 6)).ok);
    response.clear();
    while (response.size() < 2)
    {
        auto part = rawEvents->bytes.pop_for(3s);
        ASSERT_TRUE(part);
        response += *part;
    }
    EXPECT_EQ(response, std::string("\x8a\0", 2));
    EXPECT_TRUE(raw.value->stop().ok);
    EXPECT_TRUE(server.value->stop().ok);
}

TEST(WebSocketTest, ServerRejectsOversizedHeadersBeforePayloadAndRemainsUsable)
{
    auto events = std::make_shared<ServerEvents>();
    auto server = WebSocketServer::Create({.maxMessageBytes = 5}, events);
    ASSERT_TRUE(server.ok && server.value->start().ok);
    for (const auto length : {6ull, 0x80000000ull, 0x7fffffffffffffffull, 0xffffffffffffffffull})
    {
        SCOPED_TRACE(length);
        RawClient peer(server.value->port());
        peer.send(handshake);
        ASSERT_TRUE(peer.headers().starts_with("HTTP/1.1 101"));
        ASSERT_TRUE(events->opened.pop_for(3s));
        const auto header = frameHeader(0x82, length, true);
        for (const char byte : header)
            peer.send(std::string(1, byte));
        EXPECT_EQ(events->errors.pop_for(3s), NetworkError::MessageTooLarge);
        EXPECT_EQ(peer.events->connections.pop_for(3s), false);
        EXPECT_TRUE(events->closed.pop_for(3s));
    }
    RawClient peer(server.value->port());
    peer.send(handshake + frame(0x81, "hello"));
    ASSERT_TRUE(peer.headers().starts_with("HTTP/1.1 101"));
    auto message = events->messages.pop_for(3s);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->bytes, "hello");
}

TEST(WebSocketTest, ServerKeepsControlFramesSeparateFromFragmentsAndEnforcesAggregateLimit)
{
    auto events = std::make_shared<ServerEvents>();
    auto server = WebSocketServer::Create({.maxMessageBytes = 5}, events);
    ASSERT_TRUE(server.ok && server.value->start().ok);
    RawClient peer(server.value->port());
    peer.send(handshake);
    ASSERT_TRUE(peer.headers().starts_with("HTTP/1.1 101"));
    // A non-zero mask split across multiple reads also exercises mask offsets.
    const std::string first("\x01\x82\x12\x34\x56\x78\x7a\x51", 8);
    for (const char byte : first)
        peer.send(std::string(1, byte));
    peer.send(frame(0x89, "!") + frame(0x8a, "ignored") + frame(0x80, "llo"));
    auto message = events->messages.pop_for(3s);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->bytes, "hello");
    EXPECT_EQ(message->type, WebSocketMessageType::Text);
    EXPECT_EQ(peer.take(3), frame(0x8a, "!", false));

    peer.send(frame(0x02, "abc") + frame(0x89, "") + frameHeader(0x80, 3, true));
    EXPECT_EQ(events->errors.pop_for(3s), NetworkError::MessageTooLarge);
    EXPECT_EQ(peer.events->connections.pop_for(3s), false);
    EXPECT_FALSE(events->messages.pop_for(50ms));
}

TEST(WebSocketTest, ServerRejectsInvalidHandshakesBeforeConnectionEvent)
{
    auto events = std::make_shared<ServerEvents>();
    auto server = WebSocketServer::Create({}, events);
    ASSERT_TRUE(server.ok && server.value->start().ok);
    struct Case
    {
        std::string from, to;
    };
    for (const auto &tc : std::vector<Case>{
             {"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n", ""},
             {"dGhlIHNhbXBsZSBub25jZQ==", "invalid"},
             {"dGhlIHNhbXBsZSBub25jZQ==", "dGhlIHNhbXBsZSBub25jZR=="},
             {"Sec-WebSocket-Version: 13\r\n", ""},
             {"Sec-WebSocket-Version: 13", "Sec-WebSocket-Version: 12"},
             {"GET /chat", "POST /chat"},
             {"HTTP/1.1", "HTTP/1.0"},
             {"Host: localhost\r\n", ""},
             {"Connection: Upgrade\r\n", ""},
             {"Upgrade: websocket\r\n", ""},
             {"Sec-WebSocket-Version: 13", "Content-Length: 1\r\nSec-WebSocket-Version: 13"},
             {"Sec-WebSocket-Version: 13", "Transfer-Encoding: chunked\r\nSec-WebSocket-Version: 13"},
             {"Sec-WebSocket-Version: 13", "sec-websocket-key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13"}})
    {
        SCOPED_TRACE(tc.from + " -> " + tc.to);
        auto request = handshake;
        request.replace(request.find(tc.from), tc.from.size(), tc.to);
        SZX::TEST::HTTP::Client peer(server.value->port());
        peer.send(request);
        EXPECT_EQ(SZX::TEST::HTTP::status(peer.read()), 400);
        EXPECT_TRUE(peer.peerClosed());
        EXPECT_EQ(server.value->connectionNum().value, 0);
    }
    EXPECT_FALSE(events->opened.pop_for(50ms));
    // Header-name casing, OWS and token lists are valid.
    RawClient valid(server.value->port());
    auto request = handshake;
    request.replace(request.find("Connection: Upgrade"), 19, "connection: keep-alive, UpGrAdE");
    const auto cut = request.size() - 1;
    valid.send(request.substr(0, cut));
    EXPECT_FALSE(events->opened.pop_for(50ms));
    valid.send(request.substr(cut) + frame(0x81, "ready"));
    EXPECT_TRUE(valid.headers().starts_with("HTTP/1.1 101"));
    ASSERT_TRUE(events->opened.pop_for(3s));
    auto message = events->messages.pop_for(3s);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->bytes, "ready");
}

TEST(WebSocketTest, ClientRejectsOversizedHeadersBeforePayload)
{
    auto signer = WebSocketServer::Create();
    ASSERT_TRUE(signer.ok && signer.value->start().ok);
    auto rawEvents           = std::make_shared<RawServerEvents>();
    rawEvents->handshakePort = signer.value->port();
    auto raw                 = TcpServer::Create({}, rawEvents);
    ASSERT_TRUE(raw.ok && raw.value->start().ok);
    for (const auto length : {6ull, 0x80000000ull, 0x7fffffffffffffffull, 0xffffffffffffffffull})
    {
        SCOPED_TRACE(length);
        auto events = std::make_shared<ClientEvents>();
        auto client = WebSocketClient::Create(
            {.url = "ws://127.0.0.1:" + std::to_string(raw.value->port()) + "/", .maxMessageBytes = 5}, events);
        ASSERT_TRUE(client.ok && client.value->start().ok);
        rawEvents->respond();
        ASSERT_EQ(events->connections.pop_for(3s), true);
        auto connection = rawEvents->opened.pop_for(3s);
        ASSERT_TRUE(connection);
        const auto header = frameHeader(0x82, length, false);
        for (const auto byte : header)
            ASSERT_TRUE(connection->send(std::string(1, byte)).ok);
        EXPECT_EQ(events->errors.pop_for(3s), NetworkError::MessageTooLarge);
        EXPECT_EQ(events->connections.pop_for(3s), false);
        EXPECT_FALSE(events->messages.pop_for(50ms));
    }
}

TEST(WebSocketTest, ClientHandlesCoalescedUpgradeFragmentsAndControlFrames)
{
    auto signer = WebSocketServer::Create();
    ASSERT_TRUE(signer.ok && signer.value->start().ok);
    auto rawEvents           = std::make_shared<RawServerEvents>();
    rawEvents->handshakePort = signer.value->port();
    rawEvents->initialFrames =
        frame(0x01, "he", false) + frame(0x89, "!", false) + frame(0x8a, "ignored", false) + frame(0x80, "llo", false);
    auto raw = TcpServer::Create({}, rawEvents);
    ASSERT_TRUE(raw.ok && raw.value->start().ok);
    auto events = std::make_shared<ClientEvents>();
    auto client = WebSocketClient::Create(
        {.url = "ws://127.0.0.1:" + std::to_string(raw.value->port()) + "/", .maxMessageBytes = 5}, events);
    ASSERT_TRUE(client.ok && client.value->start().ok);
    rawEvents->respond();
    ASSERT_EQ(events->connections.pop_for(3s), true);
    auto message = events->messages.pop_for(3s);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->bytes, "hello");
    EXPECT_EQ(message->type, WebSocketMessageType::Text);
    std::string pong;
    while (pong.size() < 7)
    {
        auto part = rawEvents->bytes.pop_for(3s);
        ASSERT_TRUE(part);
        pong += *part;
    }
    ASSERT_EQ(pong.size(), 7);
    EXPECT_EQ(static_cast<unsigned char>(pong[0]), 0x8a);
    EXPECT_EQ(static_cast<unsigned char>(pong[1]), 0x81);
    EXPECT_EQ(static_cast<char>(pong[6] ^ pong[2]), '!');
    auto connection = rawEvents->opened.pop_for(3s);
    ASSERT_TRUE(connection);
    ASSERT_TRUE(connection->send(frame(0x02, "abc", false) + frameHeader(0x80, 3, false)).ok);
    EXPECT_EQ(events->errors.pop_for(3s), NetworkError::MessageTooLarge);
    EXPECT_EQ(events->connections.pop_for(3s), false);
}

TEST(WebSocketTest, ClientRejectsInvalidUpgradeFields)
{
    auto signer = WebSocketServer::Create();
    ASSERT_TRUE(signer.ok && signer.value->start().ok);
    for (const std::string field : {"Upgrade", "Connection", "Sec-WebSocket-Accept"})
    {
        SCOPED_TRACE(field);
        auto rawEvents           = std::make_shared<RawServerEvents>();
        rawEvents->handshakePort = signer.value->port();
        rawEvents->responseEdit  = [field](std::string &response)
        {
            const auto begin = response.find(field + ":");
            response.erase(begin, response.find("\r\n", begin) + 2 - begin);
        };
        auto raw = TcpServer::Create({}, rawEvents);
        ASSERT_TRUE(raw.ok && raw.value->start().ok);
        auto events = std::make_shared<ClientEvents>();
        auto client = WebSocketClient::Create({.url = "ws://127.0.0.1:" + std::to_string(raw.value->port()) + "/"}, events);
        ASSERT_TRUE(client.ok && client.value->start().ok);
        rawEvents->respond();
        EXPECT_EQ(events->errors.pop_for(3s), NetworkError::ProtocolError);
        EXPECT_EQ(events->connections.pop_for(3s), false);
        EXPECT_FALSE(client.value->isConnected());
    }
}
