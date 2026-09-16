#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
#include "FileHelper.hpp"
#include "SzxNetwork/HttpServer.h"

#include "../support/HttpClient.h"

using namespace SZX::NETWORK;
using namespace std::chrono_literals;
static_assert(std::is_same_v<HttpServer, SZX::NETWORK::v1::HttpServer>);

namespace
{
using namespace SZX::TEST::HTTP;

auto makeServer(std::size_t limit = 1024) -> std::unique_ptr<HttpServer>
{
    HttpServerOptions options;
    options.ioThreadCount = 2;
    options.maxBodyBytes  = limit;
    auto result           = HttpServer::Create(options);
    if (!result.ok)
        throw std::runtime_error("Create failed");
    return std::move(result.value);
}
auto ok(const HttpServerRequest &) -> HttpServerResponse
{
    return {.body = "ok"};
}
} // namespace

TEST(NetworkTypesTest, HeadersAreAsciiCaseInsensitive)
{
    HttpHeaders headers;
    headers["Content-Type"] = "first";
    headers["content-type"] = "second";
    EXPECT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers.find(std::string_view{"CONTENT-TYPE"})->second, "second");
}

TEST(HttpServerTest, ValidatesOptionsAndRoutesWithoutOpeningSockets)
{
    for (const auto threads : {0u, 65u})
    {
        HttpServerOptions options;
        options.ioThreadCount = threads;
        EXPECT_EQ(HttpServer::Create(options).error, static_cast<int>(NetworkError::InvalidArgument));
    }
    HttpServerOptions options;
    options.maxBodyBytes = 0;
    EXPECT_FALSE(HttpServer::Create(options).ok);
    options             = {};
    options.listen.host = std::string(64, 'a');
    EXPECT_FALSE(HttpServer::Create(options).ok);
#if !SZX_NETWORK_TEST_TLS
    options             = {};
    options.tls.enabled = true;
    EXPECT_EQ(HttpServer::Create(options).error, static_cast<int>(NetworkError::TlsUnavailable));
#endif
    auto server = makeServer();
    EXPECT_FALSE(server->running());
    EXPECT_EQ(server->port(), 0);
    for (const auto path : {"relative", "/{", "/{}", "/a/{id}/{id}", "/a/*/b", "/:id", "/a?b", "/a/../b"})
    {
        SCOPED_TRACE(path);
        EXPECT_EQ(server->get(path, ok).error, static_cast<int>(NetworkError::InvalidArgument));
    }
    EXPECT_FALSE(server->route("get", "/", ok).ok);
    EXPECT_FALSE(server->get("/empty", {}).ok);
    EXPECT_FALSE(server->use({}).ok);
    EXPECT_FALSE(server->onNotFound({}).ok);
    EXPECT_TRUE(server->get("/users/{id}", ok).ok);
    EXPECT_EQ(server->get("/users/{other}", ok).error, static_cast<int>(NetworkError::RouteConflict));
    EXPECT_TRUE(server->post("/users/{id}", ok).ok);
}

TEST(HttpServerTest, RoutesPreserveParametersAndMiddlewareOrder)
{
    auto server = makeServer();
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            input.attributes["order"] += "A";
                            input.userContext          = std::make_shared<int>(42);
                            auto response              = next(input);
                            response.body             += "a";
                            return response;
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            input.attributes["order"] += "B";
                            auto response              = next(input);
                            response.body             += "b";
                            return response;
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->get("/users/{id}",
                          [](const auto &input)
                          {
                              EXPECT_EQ(input.query.at("id"), "query");
                              EXPECT_EQ(input.query.at("term"), "hello world");
                              EXPECT_EQ(input.headers.at("x-token"), "token");
                              EXPECT_EQ(*std::static_pointer_cast<int>(input.userContext), 42);
                              EXPECT_EQ(input.peer.host, "127.0.0.1");
                              EXPECT_NE(input.peer.port, 0);
                              return HttpServerResponse{.body = input.attributes.at("order") + input.pathParams.at("id")};
                          })
                    .ok);
    EXPECT_TRUE(server->get("/users/me", [](const auto &) { return HttpServerResponse{.body = "literal"}; }).ok);
    EXPECT_TRUE(server->get("/files/*", ok).ok);
    EXPECT_TRUE(server->onNotFound([](const auto &) { return HttpServerResponse{.statusCode = 404, .body = "missing"}; }).ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(body(request(server->port(), "GET", "/users/a%20b?id=query&term=hello%20world", {}, "X-Token: token\r\n")),
              "ABa bba");
    EXPECT_EQ(body(request(server->port(), "GET", "/users/me")), "literalba");
    EXPECT_EQ(body(request(server->port(), "GET", "/files/a/b")), "okba");
    EXPECT_EQ(body(request(server->port(), "GET", "/missing")), "missingba");
    EXPECT_EQ(status(request(server->port(), "POST", "/users/me")), 405);
    const auto head = request(server->port(), "HEAD", "/users/me");
    EXPECT_EQ(status(head), 200);
    EXPECT_TRUE(body(head).empty());
    EXPECT_NE(head.find("Content-Length: 9"), head.npos);
}

TEST(HttpServerTest, BeforeRoutingMiddlewareRewritesBeforeSelectionAndPreservesExistingMiddlewareContract)
{
    auto server = makeServer();
    EXPECT_FALSE(server->useBeforeRouting({}).ok);
    EXPECT_TRUE(server
                    ->useBeforeRouting(
                        [](auto &input, const auto &next)
                        {
                            EXPECT_TRUE(input.pathParams.empty());
                            if (input.path == "/blocked")
                                return HttpServerResponse{.statusCode = 403};
                            if (input.path == "/entry")
                                input.path = "/items/a%20b";
                            input.attributes["order"]    = "A";
                            auto response                = next(input);
                            response.headers["X-Before"] = "yes";
                            return response;
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->useBeforeRouting(
                        [](auto &input, const auto &next)
                        {
                            input.attributes["order"] += "B";
                            return next(input);
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            if (input.method == "GET" && input.path.starts_with("/items/"))
                                EXPECT_EQ(input.pathParams.at("id"), "a b");
                            input.attributes["order"] += "C";
                            return next(input);
                        })
                    .ok);
    EXPECT_TRUE(server->get("/entry", [](const auto &) { return HttpServerResponse{.body = "wrong route"}; }).ok);
    EXPECT_TRUE(server
                    ->get("/items/{id}", [](const auto &input)
                          { return HttpServerResponse{.body = input.attributes.at("order") + ":" + input.query.at("url")}; })
                    .ok);
    EXPECT_TRUE(server->get("/raw//path", ok).ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(body(request(server->port(), "GET", "/entry?url=https://example.test//file")), "ABC:https://example.test//file");
    EXPECT_EQ(status(request(server->port(), "POST", "/entry")), 405);
    EXPECT_EQ(status(request(server->port(), "GET", "/blocked")), 403);
    EXPECT_EQ(body(request(server->port(), "GET", "/raw//path")), "ok");
    EXPECT_EQ(server->useBeforeRouting([](auto &input, const auto &next) { return next(input); }).error,
              static_cast<int>(NetworkError::AlreadyRunning));
}

TEST(HttpServerTest, HandlesMethodsBinaryBodiesAndShortCircuit)
{
    auto server = makeServer();
    std::atomic<int> calls{0};
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            if (input.path == "/blocked")
                                return HttpServerResponse{.statusCode = 401};
                            return next(input);
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->get("/blocked",
                          [&](const auto &)
                          {
                              ++calls;
                              return HttpServerResponse{};
                          })
                    .ok);
    for (const auto method : {"POST", "PUT", "PATCH", "DELETE", "OPTIONS", "TRACE"})
        EXPECT_TRUE(server->route(method, "/echo", [](const auto &input) { return HttpServerResponse{.body = input.body}; }).ok);
    EXPECT_TRUE(server->start().ok);
    const std::string payload{"a\0b", 3};
    for (const auto method : {"POST", "PUT", "PATCH", "DELETE", "OPTIONS", "TRACE"})
        EXPECT_EQ(body(request(server->port(), method, "/echo", payload)), payload);
    EXPECT_EQ(status(request(server->port(), "GET", "/blocked")), 401);
    EXPECT_EQ(calls.load(), 0);
}

TEST(HttpServerTest, RejectsOversizedBodiesBeforeTheClientFinishesSending)
{
    auto server = makeServer(4);
    std::atomic<int> calls{0};
    std::atomic<int> errors{0};
    EXPECT_TRUE(server
                    ->post("/echo",
                           [&](const auto &input)
                           {
                               ++calls;
                               return HttpServerResponse{.body = input.body};
                           })
                    .ok);
    EXPECT_TRUE(server
                    ->onError(
                        [&](auto error, const auto &)
                        {
                            EXPECT_EQ(error, NetworkError::MessageTooLarge);
                            ++errors;
                        })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(body(request(server->port(), "POST", "/echo", "1234")), "1234");
    for (const auto path : {"/echo", "/unregistered"})
    {
        Client client(server->port());
        // No body is sent. A post-read size check would time out here.
        client.send(std::string{"POST "} + path +
                    " HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000000\r\nExpect: 100-continue\r\n\r\n");
        EXPECT_EQ(status(client.read()), 413);
        Client chunked(server->port());
        chunked.send(std::string{"POST "} + path +
                     " HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n");
        chunked.send("2\r\nde\r\n"); // Deliberately omit the terminating chunk.
        EXPECT_EQ(status(chunked.read()), 413);
    }
    EXPECT_TRUE(server->stop().ok);
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(errors.load(), 4);
}

TEST(HttpServerTest, AcceptsChunkedAndContinueRequestsWithinLimit)
{
    auto server = makeServer(4);
    EXPECT_TRUE(server->post("/echo", [](const auto &input) { return HttpServerResponse{.body = input.body}; }).ok);
    EXPECT_TRUE(server->start().ok);
    Client chunked(server->port());
    chunked.send("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nab\r\n2\r\ncd\r\n0\r\n\r\n");
    EXPECT_EQ(body(chunked.read()), "abcd");
    for (const auto expect : {"100-continue", "100-CONTINUE", "100-CoNtInUe"})
    {
        SCOPED_TRACE(expect);
        Client continued(server->port());
        continued.send(std::string{"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nExpect: "} + expect +
                       "\r\n\r\n");
        EXPECT_EQ(status(continued.read(true)), 100);
        continued.send("abcd");
        EXPECT_EQ(body(continued.read()), "abcd");
    }
    Client unsupported(server->port());
    unsupported.send("POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nExpect: other\r\n\r\n");
    EXPECT_EQ(status(unsupported.read()), 417);
}

TEST(HttpServerTest, RejectsChunkLengthsBeforeReadingOrAllocatingTheChunk)
{
    auto server = makeServer(4);
    std::atomic<int> calls{0};
    std::atomic<int> sizeErrors{0};
    std::atomic<int> protocolErrors{0};
    EXPECT_TRUE(server
                    ->post("/echo",
                           [&](const auto &)
                           {
                               ++calls;
                               return HttpServerResponse{};
                           })
                    .ok);
    EXPECT_TRUE(server->get("/ok", ok).ok);
    EXPECT_TRUE(server
                    ->onError(
                        [&](auto error, const auto &)
                        {
                            if (error == NetworkError::MessageTooLarge)
                                ++sizeErrors;
                            else
                            {
                                EXPECT_EQ(error, NetworkError::ProtocolError);
                                ++protocolErrors;
                            }
                        })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    struct LengthCase
    {
        const char *length;
        int status;
    };
    for (const auto path : {"/echo", "/unregistered"})
    {
        // The bundled parser rejects its uint64_t sentinel before the chunk callback.
        for (const auto &[length, expectedStatus] : {LengthCase{"5", 413},
                                                     {"7fffffff", 413},
                                                     {"80000000", 413},
                                                     {"ffffffff", 413},
                                                     {"100000000", 413},
                                                     {"ffffffffffffffff", 400}})
        {
            SCOPED_TRACE(std::string{path} + ": " + length);
            Client client(server->port());
            client.send(std::string{"POST "} + path + " HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n" +
                        length + "\r\n");
            EXPECT_EQ(status(client.read()), expectedStatus); // No chunk body or terminating chunk is sent.
            EXPECT_EQ(body(request(server->port(), "GET", "/ok")), "ok");
        }
    }
    Client cumulative(server->port());
    cumulative.send("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n2\r\n");
    EXPECT_EQ(status(cumulative.read()), 413); // Second chunk exceeds the remaining one-byte budget.
    for (const auto &[length, expectedStatus] : {LengthCase{"4294967296", 413}, {"18446744073709551615", 400}})
    {
        SCOPED_TRACE(length);
        Client declared(server->port());
        declared.send(std::string{"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: "} + length + "\r\n\r\n");
        EXPECT_EQ(status(declared.read()), expectedStatus);
    }
    EXPECT_TRUE(server->stop().ok);
    EXPECT_EQ(calls.load(), 0);
    EXPECT_EQ(sizeErrors.load(), 12);
    EXPECT_EQ(protocolErrors.load(), 3);
}

TEST(HttpServerTest, AcceptsFragmentedRequestLinesHeadersAndBodies)
{
    auto server = makeServer();
    EXPECT_TRUE(server
                    ->post("/echo",
                           [](const auto &input)
                           {
                               EXPECT_EQ(input.headers.at("X-Fragment"), "split-value");
                               return HttpServerResponse{.body = input.body};
                           })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    const std::string wire = "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nX-Fragment: split-value\r\n\r\nabcd";
    for (const std::size_t split : {1u, 5u, 13u, 36u, 76u, 82u})
    {
        SCOPED_TRACE(split);
        Client client(server->port());
        client.send(wire.substr(0, split));
        // Deliberately separate TCP reads; no production timing depends on this delay.
        std::this_thread::sleep_for(30ms);
        client.send(wire.substr(split));
        EXPECT_EQ(body(client.read()), "abcd");
    }
}

TEST(HttpServerTest, HeadPipelinePreservesResponseOrderAndRequestState)
{
    auto server = makeServer();
    std::atomic<int> calls{0};
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            // Mutating application state must not change wire framing.
                            if (input.method == "HEAD")
                                input.method = "GET";
                            return next(input);
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->get("/ok",
                          [&](const auto &input)
                          {
                              ++calls;
                              EXPECT_EQ(input.query.at("id"), input.headers.at("X-Id"));
                              return HttpServerResponse{.body    = input.query.at("id"),
                                                        .headers = {{"Content-Type", "text/plain"}}};
                          })
                    .ok);
    EXPECT_TRUE(server
                    ->post("/echo",
                           [&](const auto &input)
                           {
                               ++calls;
                               EXPECT_FALSE(input.headers.contains("X-Id"));
                               EXPECT_TRUE(input.query.empty());
                               return HttpServerResponse{.body = input.body};
                           })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    Client client(server->port());
    client.send("HEAD /ok?id=one HTTP/1.1\r\nHost: localhost\r\nX-Id: one\r\n\r\n"
                "GET /ok?id=two HTTP/1.1\r\nHost: localhost\r\nX-Id: two\r\n\r\n"
                "HEAD /ok?id=three HTTP/1.1\r\nHost: localhost\r\nX-Id: three\r\n\r\n"
                "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nConnection: close\r\n\r\nlast");
    const auto first = client.read(true);
    EXPECT_EQ(status(first), 200);
    EXPECT_TRUE(body(first).empty());
    EXPECT_NE(first.find("Content-Length: 3"), first.npos);
    EXPECT_NE(first.find("Content-Type: text/plain"), first.npos);
    EXPECT_EQ(body(client.read()), "two");
    const auto third = client.read(true);
    EXPECT_EQ(status(third), 200);
    EXPECT_TRUE(body(third).empty());
    EXPECT_NE(third.find("Content-Length: 5"), third.npos);
    EXPECT_EQ(body(client.read()), "last");
    EXPECT_TRUE(server->stop().ok);
    EXPECT_EQ(calls.load(), 4);
}

TEST(HttpServerTest, PreservesTargetPathsWithoutRemappingAsteriskToRoot)
{
    auto server = makeServer();
    EXPECT_TRUE(server->get("/", ok).ok);
    EXPECT_TRUE(server->route("OPTIONS", "/", ok).ok);
    EXPECT_TRUE(server
                    ->onNotFound(
                        [](const auto &input)
                        {
                            const auto query = input.query.find("q");
                            return HttpServerResponse{
                                .statusCode = 404, .body = input.path + ":" + (query == input.query.end() ? "" : query->second)};
                        })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(body(request(server->port(), "GET", "/a/http://example.test/b?q=value")), "/a/http://example.test/b:value");
    EXPECT_EQ(body(request(server->port(), "GET", "/encoded%2Fpath?q=a%20b#ignored")), "/encoded%2Fpath:a b");
    EXPECT_EQ(body(request(server->port(), "GET", "/missing#ignored")), "/missing:");
    EXPECT_EQ(body(request(server->port(), "GET", "http://localhost/absolute?q=value")), "/absolute:value");
    const auto asterisk = request(server->port(), "OPTIONS", "*");
    EXPECT_EQ(status(asterisk), 404);
    EXPECT_EQ(body(asterisk), "*:");
}

TEST(HttpServerTest, RejectsMalformedFramingAndDoesNotPromoteTrailersToHeaders)
{
    auto server = makeServer();
    EXPECT_TRUE(server
                    ->post("/echo", [](const auto &input)
                           { return HttpServerResponse{.body = input.headers.at("Authorization") + ":" + input.body}; })
                    .ok);
    EXPECT_TRUE(server->get("/ok", ok).ok);
    EXPECT_TRUE(server->start().ok);
    for (const auto headers :
         {"Content-Length: 1\r\nContent-Length: 2\r\n", "Content-Length: 1\r\nTransfer-Encoding: chunked\r\n"})
    {
        Client client(server->port());
        client.send(std::string{"POST /echo HTTP/1.1\r\nHost: localhost\r\n"} + headers + "\r\n");
        EXPECT_EQ(status(client.read()), 400);
        EXPECT_EQ(body(request(server->port(), "GET", "/ok")), "ok");
    }
    Client trailers(server->port());
    trailers.send("POST /echo HTTP/1.1\r\nHost: localhost\r\nAuthorization: original\r\nTransfer-Encoding: chunked\r\n\r\n"
                  "1\r\na\r\n0\r\nAuthorization: trailer\r\n\r\n"
                  "GET /ok HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    EXPECT_EQ(body(trailers.read()), "original:a");
    EXPECT_EQ(body(trailers.read()), "ok");
}

TEST(HttpServerTest, ContainsHandlerMiddlewareAndErrorObserverExceptions)
{
    auto server = makeServer();
    std::atomic<int> errors{0};
    EXPECT_TRUE(server
                    ->onError(
                        [&](auto error, const auto &message)
                        {
                            EXPECT_EQ(error, NetworkError::Internal);
                            EXPECT_TRUE(message.empty());
                            ++errors;
                            throw 17;
                        })
                    .ok);
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            if (input.path == "/middleware")
                                throw std::runtime_error("SECRET");
                            return next(input);
                        })
                    .ok);
    EXPECT_TRUE(server->get("/standard", [](const auto &) -> HttpServerResponse { throw std::runtime_error("SECRET"); }).ok);
    EXPECT_TRUE(server->get("/unknown", [](const auto &) -> HttpServerResponse { throw 42; }).ok);
    EXPECT_TRUE(server->get("/ok", ok).ok);
    EXPECT_TRUE(server->start().ok);
    for (const auto path : {"/standard", "/unknown", "/middleware"})
    {
        const auto response = request(server->port(), "GET", path);
        EXPECT_EQ(status(response), 500);
        EXPECT_EQ(response.find("SECRET"), response.npos);
    }
    EXPECT_EQ(body(request(server->port(), "GET", "/ok")), "ok");
    EXPECT_TRUE(server->stop().ok);
    EXPECT_EQ(errors.load(), 3);
}

TEST(HttpServerTest, InvalidResponsesCannotInjectHeadersOrOverrideFraming)
{
    auto server = makeServer();
    EXPECT_TRUE(server->get("/status", [](const auto &) { return HttpServerResponse{.statusCode = 999}; }).ok);
    EXPECT_TRUE(
        server->get("/header", [](const auto &) { return HttpServerResponse{.headers = {{"X-Test", "ok\r\nInjected: true"}}}; })
            .ok);
    EXPECT_TRUE(server
                    ->get("/framing",
                          [](const auto &)
                          {
                              return HttpServerResponse{
                                  .body = "hello", .headers = {{"Content-Length", "1000"}, {"Transfer-Encoding", "chunked"}}};
                          })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(status(request(server->port(), "GET", "/status")), 500);
    const auto injected = request(server->port(), "GET", "/header");
    EXPECT_EQ(status(injected), 500);
    EXPECT_EQ(injected.find("Injected"), injected.npos);
    EXPECT_EQ(body(request(server->port(), "GET", "/framing")), "hello");
}

TEST(HttpServerTest, LifecycleRejectsCallbackStopAndAllowsRestart)
{
    auto server = makeServer();
    EXPECT_TRUE(server
                    ->get("/stop",
                          [&](const auto &)
                          {
                              const auto result = server->stop();
                              EXPECT_EQ(result.error, static_cast<int>(NetworkError::Internal));
                              EXPECT_FALSE(server->get("/late", ok).ok);
                              return HttpServerResponse{.body = "alive"};
                          })
                    .ok);
    EXPECT_TRUE(server->stop().ok);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(server->start().ok);
        EXPECT_TRUE(server->running());
        EXPECT_NE(server->port(), 0);
        EXPECT_EQ(server->start().error, static_cast<int>(NetworkError::AlreadyRunning));
        EXPECT_EQ(server->get("/late", ok).error, static_cast<int>(NetworkError::AlreadyRunning));
        EXPECT_EQ(body(request(server->port(), "GET", "/stop")), "alive");
        EXPECT_TRUE(server->stop().ok);
        EXPECT_FALSE(server->running());
        EXPECT_EQ(server->port(), 0);
        EXPECT_TRUE(server->stop().ok);
    }
}

TEST(HttpServerTest, PortConflictIsRecoverableAndDestructorReleasesPort)
{
    auto first = makeServer();
    EXPECT_TRUE(first->start().ok);
    const auto port = first->port();
    HttpServerOptions options;
    options.listen        = {"127.0.0.1", port};
    options.ioThreadCount = 1;
    auto second           = HttpServer::Create(options);
    EXPECT_TRUE(second.ok);
    EXPECT_EQ(second.value->start().error, static_cast<int>(NetworkError::BindFailed));
    EXPECT_FALSE(second.value->running());
    first.reset();
    EXPECT_TRUE(second.value->start().ok);
    EXPECT_EQ(second.value->port(), port);
}

TEST(HttpServerTest, KeepAliveAndConcurrentRequests)
{
    auto server = makeServer();
    EXPECT_TRUE(server->get("/ok", ok).ok);
    EXPECT_TRUE(server->start().ok);
    Client client(server->port());
    for (int i = 0; i < 3; ++i)
    {
        client.send("HEAD /ok HTTP/1.1\r\nHost: localhost\r\n\r\n");
        const auto head = client.read(true);
        EXPECT_EQ(status(head), 200);
        EXPECT_NE(head.find("Content-Length: 2"), head.npos);
        EXPECT_TRUE(body(head).empty());
        client.send("GET /ok HTTP/1.1\r\nHost: localhost\r\n\r\n");
        EXPECT_EQ(body(client.read()), "ok");
    }
    std::vector<std::future<std::string>> results;
    for (int i = 0; i < 8; ++i)
        results.push_back(std::async(std::launch::async, [&] { return request(server->port(), "GET", "/ok"); }));
    for (auto &result : results)
        EXPECT_EQ(body(result.get()), "ok");
    EXPECT_TRUE(server->stop().ok); // Also closes the still-open keepalive client.
}

TEST(HttpServerTest, InjectedErrorResponsesPreserveStatusAndContainCallbackFailures)
{
    auto server = makeServer(4);
    std::atomic<int> observed{0};
    EXPECT_TRUE(server->onError([&](auto, const auto &) { ++observed; }).ok);
    EXPECT_TRUE(server
                    ->onRequestError(
                        [&](const auto &input, int code, auto error)
                        {
                            EXPECT_EQ(code, 413);
                            EXPECT_EQ(error, NetworkError::MessageTooLarge);
                            EXPECT_EQ(input.headers.at("X-Custom"), "retained");
                            EXPECT_FALSE(server->stop().ok);
                            return HttpServerResponse{.statusCode = 200, .body = "custom", .headers = {{"X-Error", "injected"}}};
                        })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    auto response = request(server->port(), "POST", "/absent", "12345", "X-Custom: retained\r\n");
    EXPECT_EQ(status(response), 413);
    EXPECT_EQ(body(response), "custom");
    EXPECT_NE(response.find("X-Error: injected"), response.npos);
    EXPECT_TRUE(server->stop().ok);
    EXPECT_EQ(observed.load(), 1);
    for (const bool throws : {false, true})
    {
        EXPECT_TRUE(server
                        ->onRequestError(
                            [throws](const auto &, int, auto) -> HttpServerResponse
                            {
                                if (throws)
                                    throw std::runtime_error("private diagnostic");
                                return {.headers = {{"X-Invalid", "value\r\nInjected: true"}}};
                            })
                        .ok);
        EXPECT_TRUE(server->start().ok);
        response = request(server->port(), "POST", "/absent", "12345");
        EXPECT_EQ(status(response), 413);
        EXPECT_TRUE(body(response).empty());
        EXPECT_EQ(response.find("Injected"), response.npos);
        EXPECT_TRUE(server->stop().ok);
    }
    EXPECT_EQ(observed.load(), 3);
}

TEST(HttpServerTest, ConnectionLimitIncludesIdleConnectionsAndSurvivesRestart)
{
    HttpServerOptions options;
    options.ioThreadCount = 4;
    auto created          = HttpServer::Create(options);
    EXPECT_TRUE(created.ok);
    auto server = std::move(created.value);
    EXPECT_EQ(server->setMaxConnections(0).error, static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_TRUE(server->setMaxConnections(1).ok);
    EXPECT_TRUE(server->get("/ok", ok).ok);
    for (int run = 0; run < 2; ++run)
    {
        EXPECT_TRUE(server->start().ok);
        EXPECT_EQ(server->setMaxConnections(2).error, static_cast<int>(NetworkError::AlreadyRunning));
        Client held(server->port());
        held.send("GET /ok HTTP/1.1\r\nHost: localhost\r\n\r\n");
        EXPECT_EQ(body(held.read()), "ok");
        Client rejected(server->port());
        EXPECT_TRUE(rejected.peerClosed());
        EXPECT_TRUE(server->stop().ok);
    }
}

TEST(HttpServerTest, StopWaitsForInFlightCallback)
{
    auto server = makeServer();
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<bool> finished{false};
    EXPECT_TRUE(server
                    ->get("/wait",
                          [&](const auto &)
                          {
                              entered.set_value();
                              released.wait(); // Intentional test-only blocking to exercise join semantics.
                              finished = true;
                              return HttpServerResponse{};
                          })
                    .ok);
    EXPECT_TRUE(server->start().ok);
    Client client(server->port());
    client.send("GET /wait HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_EQ(entered.get_future().wait_for(3s), std::future_status::ready);
    auto stopped = std::async(std::launch::async, [&] { return server->stop(); });
    EXPECT_EQ(stopped.wait_for(30ms), std::future_status::timeout);
    release.set_value();
    EXPECT_TRUE(stopped.get().ok);
    EXPECT_TRUE(finished.load());
}

TEST(HttpServerTest, StaticMountIsConfinedAndFallbackStaysWithinPrefix)
{
    // An ephemeral port makes this test directory unique across concurrent test binaries.
    auto server = makeServer();
    EXPECT_TRUE(server->start().ok);
    const auto directory = std::filesystem::temp_directory_path() / ("szx-network-" + std::to_string(server->port()));
    EXPECT_TRUE(server->stop().ok);
    std::filesystem::create_directories(directory / "public");
    struct Cleanup final
    {
        std::filesystem::path directory;
        ~Cleanup()
        {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    } cleanup{directory};
    const auto root = directory / "public";
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "index.html", "<html>app</html>"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "hello.txt", "hello"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "custom.htm", "custom index"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(directory / "secret.txt", "SECRET"));
    const auto files = HttpServer::CreateStaticHandler(root, "custom.htm");
    EXPECT_TRUE(files.ok);
    EXPECT_FALSE(HttpServer::CreateStaticHandler(root, "../secret.txt").ok);
    EXPECT_EQ(files.value(HttpServerRequest{.method = "GET", .path = "/"}).body, "custom index");
    EXPECT_EQ(files.value(HttpServerRequest{.method = "GET", .path = "/missing"}).statusCode, 404);
    EXPECT_EQ(files.value(HttpServerRequest{.method = "GET", .path = "/%2e%2e/secret.txt"}).statusCode, 403);
    EXPECT_TRUE(server->mountStatic("/ui", root, "index.html").ok);
    EXPECT_EQ(server->mountStatic("/ui/", root).error, static_cast<int>(NetworkError::RouteConflict));
    EXPECT_FALSE(server->mountStatic("/bad", root, "../secret.txt").ok);
    EXPECT_TRUE(server
                    ->use(
                        [](auto &input, const auto &next)
                        {
                            auto response                    = next(input);
                            response.headers["X-Middleware"] = "yes";
                            return response;
                        })
                    .ok);
    std::error_code symlinkError;
    std::filesystem::create_symlink(directory / "secret.txt", root / "escape.txt", symlinkError);
    EXPECT_TRUE(server->start().ok);
    EXPECT_EQ(body(request(server->port(), "GET", "/ui/hello.txt")), "hello");
    const auto fallback = request(server->port(), "GET", "/ui/client/route");
    EXPECT_EQ(body(fallback), "<html>app</html>");
    EXPECT_NE(fallback.find("X-Middleware: yes"), fallback.npos);
    EXPECT_EQ(status(request(server->port(), "GET", "/api/missing")), 404);
    EXPECT_EQ(status(request(server->port(), "GET", "/uix/missing")), 404);
    EXPECT_EQ(status(request(server->port(), "POST", "/ui/hello.txt")), 404);
    for (const auto path : {"/ui/../secret.txt", "/ui/%2e%2e/secret.txt", "/ui/%2f../secret.txt", "/ui/C%3a/secret.txt",
                            "/ui/hello.txt%3asecret", "/ui/%00"})
    {
        SCOPED_TRACE(path);
        const auto response = request(server->port(), "GET", path);
        EXPECT_EQ(status(response), 403);
        EXPECT_EQ(response.find("SECRET"), response.npos);
    }
    if (!symlinkError)
        EXPECT_EQ(status(request(server->port(), "GET", "/ui/escape.txt")), 403);
    EXPECT_TRUE(server->stop().ok);
}
