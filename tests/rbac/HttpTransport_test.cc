#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "../../RBAC/szx_admin/src/Routes.h"
#include "../support/HttpClient.h"
#include "FileHelper.hpp"
#include "Serializer.hpp"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RequestContext.h"
#include "SzxAdmin/Infra/RestApi.h"
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

void expectError(const std::string &response, int expectedStatus, int code, std::string_view id = "test-id")
{
    EXPECT_EQ(status(response), expectedStatus);
    const auto json = SZX::COMMON::Json::parse(body(response));
    EXPECT_TRUE(json.at("code").is_number_integer());
    EXPECT_EQ(json.at("code"), code);
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
    expectError(request(transport.port(), "GET", "/api/v1/echo", {}, idHeader), 405, 2219);
    expectError(request(transport.port(), "GET", "/api/v1/absent", {}, idHeader), 404, 2205);
    for (const auto path : {"/api/v1/throws", "/api/v1/bad-header"})
    {
        const auto response = request(transport.port(), "GET", path, {}, idHeader);
        expectError(response, 500, 2299);
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

TEST(AdminHttpIntegrationTest, ReusesNetworkResponseHeadersWithoutCaseSensitiveDuplicates)
{
    HttpTransport transport(testConfig());
    ASSERT_TRUE(
        transport
            .registerApiRoute("GET", "/response-headers",
                              [](const auto &)
                              {
                                  return SZX::NETWORK::HttpServerResponse{
                                      .body    = "payload",
                                      .headers = {{"content-type", "text/plain"}, {"x-request-id", "untrusted-handler-id"}}};
                              })
            .ok);
    ASSERT_TRUE(transport.start().ok);
    const auto response = request(transport.port(), "GET", "/api/v1/response-headers", {}, idHeader);
    EXPECT_EQ(status(response), 200);
    EXPECT_EQ(body(response), "payload");
    EXPECT_NE(response.find("text/plain"), response.npos);
    EXPECT_EQ(response.find("application/json"), response.npos);
    EXPECT_EQ(response.find("untrusted-handler-id"), response.npos);
    EXPECT_NE(response.find("test-id"), response.npos);
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
        expectError(client.read(), 413, 2215);
        Client chunked(transport.port());
        chunked.send(std::string{"POST "} + path + " HTTP/1.1\r\nHost: localhost\r\n" + idHeader +
                     "Transfer-Encoding: chunked\r\n\r\n80000000\r\n");
        expectError(chunked.read(), 413, 2215);
    }
    Client expectation(transport.port());
    expectation.send(std::string{"POST /api/v1/echo HTTP/1.1\r\nHost: localhost\r\n"} + idHeader +
                     "Content-Length: 4\r\nExpect: unsupported\r\n\r\n");
    expectError(expectation.read(), 417, 2206);
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
            expectError(request(transport.port(), "GET", path, {}, idHeader), 404, 2205);
        const auto spa = request(transport.port(), "GET", "/dashboard/users", {}, idHeader);
        EXPECT_EQ(status(spa), fallback ? 200 : 404);
        if (fallback)
            EXPECT_EQ(body(spa), "<html>app</html>");
        for (const auto path : {"/%2e%2e/secret.txt", "/%2e%2e/secret", "/C%3a/secret", "/%00"})
        {
            const auto unsafe = request(transport.port(), "GET", path, {}, idHeader);
            expectError(unsafe, 403, 2204);
            EXPECT_EQ(unsafe.find("SECRET"), unsafe.npos);
        }
        expectError(request(transport.port(), "POST", "/asset.js", {}, idHeader), 404, 2205);
        EXPECT_TRUE(transport.stop().ok);
    }
}

TEST(AdminHttpIntegrationTest, ConfiguredHealthPathSupportsExtraSlashesWithoutAddingRootAlias)
{
    WebRoot files;
    const auto root = files.directory / "public";
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "index.html", "SPA"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "health", "STATIC"));
    for (const std::string prefix : {"/api/v1", "/custom", "/"})
    {
        SCOPED_TRACE(prefix);
        auto config       = testConfig();
        config.staticRoot = root.string();
        config.apiPrefix  = prefix;
        HttpTransport transport(config);
        EXPECT_TRUE(transport.start().ok);
        const auto prefixed = prefix == "/" ? "/health" : prefix + "/health";
        auto repeated       = prefixed;
        for (std::size_t i = 0; i < repeated.size(); ++i)
            if (repeated[i] == '/')
            {
                repeated.insert(i, "//");
                i += 2;
            }
        for (const auto &path : {prefixed, prefixed + "/", "/" + prefixed, repeated, repeated + "///"})
        {
            SCOPED_TRACE(path);
            const auto response = request(transport.port(), "GET", path, {}, idHeader);
            EXPECT_EQ(status(response), 200);
            const auto json = SZX::COMMON::Json::parse(body(response));
            EXPECT_EQ(json.at("data").at("status"), "ok");
            EXPECT_EQ(json.at("request_id"), "test-id");
            const auto head = request(transport.port(), "HEAD", path, {}, idHeader);
            EXPECT_EQ(status(head), 200);
            EXPECT_TRUE(body(head).empty());
            expectError(request(transport.port(), "POST", path, {}, idHeader), 405, 2219);
        }
        if (prefix != "/")
            EXPECT_EQ(body(request(transport.port(), "GET", "/health")), "STATIC");
        EXPECT_TRUE(transport.stop().ok);
    }
}

TEST(AdminHttpIntegrationTest, NormalizesAllRoutePathsBeforeMatchingAndBeforeApiStaticPolicy)
{
    WebRoot files;
    const auto root = files.directory / "public";
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "index.html", "SPA"));
    EXPECT_TRUE(SZX::COMMON::FileHelper::WriteText(root / "api" / "v1" / "private", "STATIC SECRET"));
    auto config       = testConfig();
    config.staticRoot = root.string();
    HttpTransport transport(config);
    EXPECT_TRUE(transport
                    .registerRoute("POST", "/api/v1/echo",
                                   [](const auto &input) { return HttpResponse{.body = input.path + ":" + input.body}; })
                    .ok);
    EXPECT_EQ(transport.registerRoute("POST", "/api///v1//echo", [](const auto &) { return HttpResponse{}; }).error,
              static_cast<int>(ErrorCode::RouteConflict));
    EXPECT_TRUE(transport.start().ok);
    const std::string payload = "https://example.test///value";
    EXPECT_EQ(body(request(transport.port(), "POST", "///api//v1///echo?url=https://example.test//value", payload)),
              "/api/v1/echo:" + payload);
    expectError(request(transport.port(), "GET", "///api//v1///echo", {}, idHeader), 405, 2219);
    expectError(request(transport.port(), "GET", "///api//v1///private", {}, idHeader), 404, 2205);
    expectError(request(transport.port(), "POST", "/api%2Fv1/echo", {}, idHeader), 404, 2205);
    expectError(request(transport.port(), "GET", "///%2e%2e//secret", {}, idHeader), 403, 2204);
    EXPECT_TRUE(transport.stop().ok);
}

TEST(AdminHttpIntegrationTest, CarriesRestPathQueryAndHeadersThroughApiRoutes)
{
    auto config      = testConfig();
    config.apiPrefix = "/service/v2";
    HttpTransport transport(config);
    EXPECT_TRUE(transport
                    .registerApiRoute("GET", "/admin/users/{id}",
                                      [](const HttpRequest &input)
                                      {
                                          EXPECT_EQ(input.header("x-request-id"), input.requestId);
                                          const auto token = RestApi::bearerToken(input);
                                          if (!token.ok)
                                              return RestApi::error(static_cast<ErrorCode>(token.error), input.requestId);
                                          return RestApi::success(SZX::COMMON::Json{{"id", input.pathParams.at("id")},
                                                                                    {"search", input.query.at("search")},
                                                                                    {"token", token.value}},
                                                                  input.requestId);
                                      })
                    .ok);
    EXPECT_FALSE(transport.registerApiRoute("GET", "admin/users", [](const auto &) { return HttpResponse{}; }).ok);
    EXPECT_TRUE(transport.start().ok);

    const auto response = request(transport.port(), "GET", "/service/v2/admin/users/user%2D1?search=Alice%20Smith", {},
                                  "authorization: Bearer test-token\r\nx-request-id: rest-request\r\n");
    EXPECT_EQ(status(response), 200);
    const auto json = SZX::COMMON::Json::parse(body(response));
    EXPECT_EQ(json.at("data").at("id"), "user-1");
    EXPECT_EQ(json.at("data").at("search"), "Alice Smith");
    EXPECT_EQ(json.at("data").at("token"), "test-token");
    EXPECT_EQ(json.at("request_id"), "rest-request");
    EXPECT_NE(response.find("Cache-Control: no-store"), response.npos);
    EXPECT_TRUE(transport.stop().ok);
}

TEST(AdminHttpIntegrationTest, ValidatesJsonAndVersionAndSupportsOptions)
{
    auto config      = testConfig();
    config.apiPrefix = "/";
    HttpTransport transport(config);
    ASSERT_TRUE(transport
                    .registerApiRoute("PATCH", "/users/{id}",
                                      [](const HttpRequest &input)
                                      {
                                          const auto json = RestApi::jsonObject(input);
                                          if (!json.ok)
                                              return RestApi::error(static_cast<ErrorCode>(json.error), input.requestId);
                                          const auto version = RestApi::ifMatch(input);
                                          if (!version.ok)
                                              return RestApi::error(static_cast<ErrorCode>(version.error), input.requestId);
                                          return RestApi::success(SZX::COMMON::Json{{"version", version.value}}, input.requestId);
                                      })
                    .ok);
    ASSERT_TRUE(transport
                    .registerApiRoute("OPTIONS", "/users/{id}",
                                      [](const HttpRequest &input)
                                      {
                                          auto response = RestApi::noContent(input.requestId);
                                          response.headers.emplace("Allow", "PATCH, OPTIONS");
                                          return response;
                                      })
                    .ok);
    ASSERT_TRUE(transport.start().ok);
    const auto headers = std::string{idHeader} + "content-type: application/json\r\nif-match: \"42\"\r\n";
    const auto valid   = request(transport.port(), "PATCH", "/users/u1", "{}", headers);
    EXPECT_EQ(status(valid), 200);
    EXPECT_EQ(SZX::COMMON::Json::parse(body(valid)).at("data").at("version"), 42);
    expectError(request(transport.port(), "PATCH", "/users/u1", "[]", headers), 422, 2206);
    expectError(
        request(transport.port(), "PATCH", "/users/u1", "{}", std::string{idHeader} + "Content-Type: application/json\r\n"), 422,
        2206);
    Client client(transport.port());
    client.send(std::string{"OPTIONS /users/u1 HTTP/1.1\r\nHost: localhost\r\n"} + idHeader + "\r\n");
    const auto options = client.read(true);
    EXPECT_EQ(status(options), 204);
    EXPECT_TRUE(body(options).empty());
    EXPECT_NE(options.find("Allow: PATCH, OPTIONS"), options.npos);
    EXPECT_NE(options.find("X-Request-ID: test-id"), options.npos);
    EXPECT_TRUE(transport.stop().ok);
}

TEST(AdminHttpIntegrationTest, RegistersServiceRoutesAndReportsUnavailableDatabase)
{
    auto config      = testConfig();
    config.apiPrefix = "/service/v2";
    DatabasePool database(DatabaseConfig{});
    HttpTransport transport(config);
    ASSERT_TRUE(SZX::ADMIN::registerRoutes(transport, database).ok);
    EXPECT_EQ(SZX::ADMIN::registerRoutes(transport, database).error, static_cast<int>(ErrorCode::RouteConflict));
    ASSERT_TRUE(transport.start().ok);
    expectError(request(transport.port(), "GET", "/service/v2/ready", {}, idHeader), 503, 2220);
    EXPECT_EQ(status(request(transport.port(), "GET", "/service/v2/health")), 200);
    expectError(request(transport.port(), "GET", "/api/v1/ready", {}, idHeader), 404, 2205);
    EXPECT_TRUE(transport.stop().ok);
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
