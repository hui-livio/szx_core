#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "../support/HttpClient.h"
#include "FileHelper.hpp"
#include "Serializer.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RequestContext.h"
#include "SzxNetwork/HttpServer.h"

using namespace SZX::ADMIN::INFRA;
using namespace SZX::TEST::HTTP;

namespace
{
auto testConfig() -> ServerConfig
{
    // Reserve an ephemeral port via the public network API, then release it for the adapter.
    auto reservation = SZX::NETWORK::HttpServer::Create({});
    if (!reservation.ok || !reservation.value->start().ok)
        throw std::runtime_error("Cannot reserve a test port");
    ServerConfig config;
    config.port          = reservation.value->port();
    config.workerThreads = 3;
    config.staticRoot.clear();
    if (!reservation.value->stop().ok)
        throw std::runtime_error("Cannot release test port");
    return config;
}

void expectError(const std::string &response, int expectedStatus, std::string_view code, std::string_view id = "test-id")
{
    EXPECT_EQ(status(response), expectedStatus);
    const auto json = SZX::COMMON::Json::parse(body(response));
    EXPECT_EQ(json.at("code").get<std::string>(), code);
    EXPECT_EQ(json.at("request_id").get<std::string>(), id);
    EXPECT_TRUE(json.at("details").is_object());
    EXPECT_NE(response.find("X-Request-ID: " + std::string{id}), response.npos);
}

struct WebRoot
{
    std::filesystem::path directory = std::filesystem::temp_directory_path() / ("szx-admin-" + newRequestId());
    WebRoot()
    {
        std::filesystem::create_directories(directory / "public");
    }
    ~WebRoot()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
constexpr auto idHeader = "x-request-id: test-id\r\n";
} // namespace

TEST(AdminHttpIntegrationTest, PreservesHealthRoutesRequestIdsAndBusinessFailures)
{
    auto config      = testConfig();
    config.accessLog = true;
    HttpTransport transport(config);
    std::atomic<int> calls{0};
    EXPECT_TRUE(transport
                    .registerRoute("POST", "/api/v1/echo",
                                   [&](const auto &input)
                                   {
                                       ++calls;
                                       EXPECT_EQ(input.requestId, "test-id");
                                       EXPECT_EQ(input.headers.at("X-Request-ID"), "test-id");
                                       EXPECT_EQ(input.headers.at("Authorization"), "Bearer test-token");
                                       return HttpResponse{.body    = input.body,
                                                           .headers = {{"Content-Type", "application/octet-stream"}}};
                                   })
                    .ok);
    EXPECT_EQ(transport.registerRoute("POST", "/api/v1/echo", [](const auto &) { return HttpResponse{}; }).error,
              static_cast<int>(ErrorCode::RouteConflict));
    EXPECT_TRUE(transport
                    .registerRoute("GET", "/api/v1/throws",
                                   [](const auto &) -> HttpResponse { throw std::runtime_error("SECRET diagnostic"); })
                    .ok);
    EXPECT_TRUE(transport
                    .registerRoute("GET", "/api/v1/bad-header",
                                   [](const auto &) { return HttpResponse{.headers = {{"X-Value", "bad\r\nInjected: true"}}}; })
                    .ok);
    EXPECT_TRUE(transport
                    .registerRoute("GET", "/api/v1/stop",
                                   [&](const auto &)
                                   {
                                       EXPECT_EQ(transport.stop().error, static_cast<int>(ErrorCode::TransportFailure));
                                       return HttpResponse{.body = "alive"};
                                   })
                    .ok);
    EXPECT_TRUE(transport.start().ok);
    const auto health = request(transport.port(), "GET", "/api/v1/health", {}, idHeader);
    EXPECT_EQ(status(health), 200);
    EXPECT_EQ(SZX::COMMON::Json::parse(body(health)).at("data").at("status"), "ok");
    EXPECT_EQ(SZX::COMMON::Json::parse(body(health)).at("request_id"), "test-id");
    const auto newIdResponse = request(transport.port(), "GET", "/api/v1/health", {}, "X-Request-ID: invalid id\r\n");
    const auto generated     = SZX::COMMON::Json::parse(body(newIdResponse)).at("request_id").get<std::string>();
    EXPECT_TRUE(isValidRequestId(generated));
    EXPECT_NE(newIdResponse.find("X-Request-ID: " + generated), newIdResponse.npos);
    const std::string bytes{"a\0b", 3};
    EXPECT_EQ(body(request(transport.port(), "POST", "/api/v1/echo", bytes,
                           std::string{idHeader} + "Authorization: Bearer test-token\r\n")),
              bytes);
    expectError(request(transport.port(), "GET", "/api/v1/echo", {}, idHeader), 405, "METHOD_NOT_ALLOWED");
    expectError(request(transport.port(), "GET", "/api/v1/absent", {}, idHeader), 404, "RESOURCE_NOT_FOUND");
    for (const auto path : {"/api/v1/throws", "/api/v1/bad-header"})
    {
        const auto response = request(transport.port(), "GET", path, {}, idHeader);
        expectError(response, 500, "INTERNAL_FAILURE");
        EXPECT_EQ(response.find("SECRET"), response.npos);
        EXPECT_EQ(response.find("Injected"), response.npos);
    }
    EXPECT_EQ(body(request(transport.port(), "GET", "/api/v1/stop")), "alive");
    EXPECT_TRUE(transport.stop().ok);
    EXPECT_EQ(calls.load(), 1);
    EXPECT_TRUE(transport.start().ok);
    EXPECT_EQ(status(request(transport.port(), "HEAD", "/api/v1/health")), 200);
    EXPECT_TRUE(transport.stop().ok);
}

TEST(AdminHttpIntegrationTest, FormatsEarlyBodyLimitAndProtocolFailuresThroughInjectedCallback)
{
    auto config                = testConfig();
    config.maxRequestBodyBytes = 4;
    HttpTransport transport(config);
    std::atomic<int> calls{0};
    EXPECT_TRUE(transport
                    .registerRoute("POST", "/api/v1/echo",
                                   [&](const auto &input)
                                   {
                                       ++calls;
                                       return HttpResponse{.body = input.body};
                                   })
                    .ok);
    EXPECT_TRUE(transport.start().ok);
    for (const auto path : {"/api/v1/echo", "/api/v1/missing"})
    {
        Client client(transport.port());
        client.send(std::string{"POST "} + path + " HTTP/1.1\r\nHost: localhost\r\n" + idHeader +
                    "Content-Length: 1000000\r\nExpect: 100-CONTINUE\r\n\r\n");
        expectError(client.read(), 413, "PAYLOAD_TOO_LARGE");
        Client chunked(transport.port());
        chunked.send(std::string{"POST "} + path + " HTTP/1.1\r\nHost: localhost\r\n" + idHeader +
                     "Transfer-Encoding: chunked\r\n\r\n80000000\r\n");
        expectError(chunked.read(), 413, "PAYLOAD_TOO_LARGE");
    }
    Client expectation(transport.port());
    expectation.send(std::string{"POST /api/v1/echo HTTP/1.1\r\nHost: localhost\r\n"} + idHeader +
                     "Content-Length: 4\r\nExpect: unsupported\r\n\r\n");
    expectError(expectation.read(), 417, "VALIDATION_ERROR");
    Client continued(transport.port());
    continued.send(std::string{"POST /api/v1/echo HTTP/1.1\r\nHost: localhost\r\n"} + idHeader +
                   "Content-Length: 4\r\nExpect: 100-CONTINUE\r\n\r\n");
    EXPECT_EQ(status(continued.read(true)), 100);
    continued.send("abcd");
    EXPECT_EQ(body(continued.read()), "abcd");
    EXPECT_TRUE(transport.stop().ok);
    EXPECT_EQ(calls.load(), 1);
}

TEST(AdminHttpIntegrationTest, KeepsStaticAndSpaPolicyOutsideNetworkAndExcludesApi)
{
    WebRoot files;
    const auto root = files.directory / "public";
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "app.htm", "<html>app</html>"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "nested" / "app.htm", "nested index"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "asset.js", "asset"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "custom" / "hidden", "NOT AN API"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(files.directory / "secret.txt", "SECRET"));
    for (const bool fallback : {false, true})
    {
        auto config           = testConfig();
        config.staticRoot     = root.string();
        config.staticHomePage = "app.htm";
        config.apiPrefix      = "/custom";
        config.spaFallback    = fallback;
        HttpTransport transport(config);
        EXPECT_TRUE(transport.start().ok);
        EXPECT_EQ(body(request(transport.port(), "GET", "/")), "<html>app</html>");
        EXPECT_EQ(body(request(transport.port(), "GET", "/nested/")), "nested index");
        EXPECT_EQ(body(request(transport.port(), "GET", "/asset.js")), "asset");
        const auto head = request(transport.port(), "HEAD", "/asset.js", {}, idHeader);
        EXPECT_EQ(status(head), 200);
        EXPECT_TRUE(body(head).empty());
        EXPECT_NE(head.find("Content-Length: 5"), head.npos);
        EXPECT_NE(head.find("X-Request-ID: test-id"), head.npos);
        EXPECT_EQ(status(request(transport.port(), "GET", "/custom/health")), 200);
        for (const auto path : {"/custom", "/custom/hidden", "/custom/missing", "/missing.js"})
            expectError(request(transport.port(), "GET", path, {}, idHeader), 404, "RESOURCE_NOT_FOUND");
        const auto spa = request(transport.port(), "GET", "/dashboard/users", {}, idHeader);
        EXPECT_EQ(status(spa), fallback ? 200 : 404);
        if (fallback)
            EXPECT_EQ(body(spa), "<html>app</html>");
        for (const auto path : {"/%2e%2e/secret.txt", "/%2e%2e/secret", "/C%3a/secret", "/%00"})
        {
            const auto unsafe = request(transport.port(), "GET", path, {}, idHeader);
            expectError(unsafe, 403, "FORBIDDEN");
            EXPECT_EQ(unsafe.find("SECRET"), unsafe.npos);
        }
        expectError(request(transport.port(), "POST", "/asset.js", {}, idHeader), 404, "RESOURCE_NOT_FOUND");
        EXPECT_TRUE(transport.stop().ok);
    }
}

TEST(AdminHttpIntegrationTest, EnforcesConfiguredConnectionLimitAndContainsBindFailures)
{
    auto config           = testConfig();
    config.maxConnections = 1;
    HttpTransport transport(config);
    EXPECT_TRUE(transport.start().ok);
    Client held(transport.port());
    held.send("GET /api/v1/health HTTP/1.1\r\nHost: localhost\r\n\r\n");
    EXPECT_EQ(status(held.read()), 200);
    Client rejected(transport.port());
    EXPECT_TRUE(rejected.peerClosed());
    HttpTransport collision(config);
    EXPECT_EQ(collision.start().error, static_cast<int>(ErrorCode::TransportFailure));
    EXPECT_FALSE(collision.running());
    EXPECT_TRUE(transport.stop().ok);
    EXPECT_TRUE(collision.start().ok);
    EXPECT_EQ(status(request(collision.port(), "GET", "/api/v1/health")), 200);
    EXPECT_TRUE(collision.stop().ok);
}
