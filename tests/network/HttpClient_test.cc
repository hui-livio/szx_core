#include "SzxNetwork/HttpClient.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include "SzxNetwork/HttpServer.h"

using namespace SZX::NETWORK;
using namespace std::chrono_literals;
static_assert(std::is_same_v<HttpClient, SZX::NETWORK::v1::HttpClient>);

#if SZX_NETWORK_TEST_HTTP_CLIENT
namespace
{
class HttpClientTest : public ::testing::Test
{
protected:
    std::unique_ptr<HttpServer> server;
    std::string base;
    void SetUp() override
    {
        auto created = HttpServer::Create({.ioThreadCount = 4});
        ASSERT_TRUE(created.ok);
        server = std::move(created.value);
        for (const auto method : {"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"})
            ASSERT_TRUE(server
                            ->route(method, "/echo",
                                    [](const auto &request)
                                    {
                                        SZX::COMMON::Json data{{"method", request.method},
                                                               {"body", request.body},
                                                               {"query", request.query},
                                                               {"headers", request.headers}};
                                        return HttpServerResponse{.body    = data.dump(),
                                                                  .headers = {{"Content-Type", "application/json"}}};
                                    })
                            .ok);
        ASSERT_TRUE(
            server->get("/failure", [](const auto &) { return HttpServerResponse{.statusCode = 422, .body = "invalid"}; }).ok);
        ASSERT_TRUE(server
                        ->get("/redirect", [](const auto &)
                              { return HttpServerResponse{.statusCode = 302, .headers = {{"Location", "/echo"}}}; })
                        .ok);
        ASSERT_TRUE(server->get("/large", [](const auto &) { return HttpServerResponse{.body = std::string(4096, 'a')}; }).ok);
        ASSERT_TRUE(server->post("/binary", [](const auto &request) { return HttpServerResponse{.body = request.body}; }).ok);
        ASSERT_TRUE(server
                        ->get("/slow",
                              [](const auto &)
                              {
                                  std::this_thread::sleep_for(1500ms);
                                  return HttpServerResponse{.body = "late"};
                              })
                        .ok);
        ASSERT_TRUE(server->get("/", [](const auto &request) { return HttpServerResponse{.body = request.query.at("x")}; }).ok);
        ASSERT_TRUE(server->start().ok);
        base = "http://127.0.0.1:" + std::to_string(server->port());
    }
};
} // namespace

TEST_F(HttpClientTest, EncodesQueryAndPreservesExistingParameters)
{
    HttpRequestOptions options;
    options.params             = {{"value", "a b&+="}, {"utf8", "\xE4\xB8\xAD"}, {"repeat", "one"}, {"repeat", "two"}};
    options.headers["X-Probe"] = "yes";
    const auto result          = http::get(base + "/echo?original=1#discard", options);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.value.ok());
    const auto json = result.value.json();
    ASSERT_TRUE(json.ok);
    EXPECT_EQ(json.value["query"]["original"], "1");
    EXPECT_EQ(json.value["query"]["value"], "a b&+=");
    EXPECT_EQ(json.value["query"]["utf8"], "\xE4\xB8\xAD");
    EXPECT_NE(result.value.url.find("repeat=one&repeat=two"), std::string::npos);
    EXPECT_EQ(result.value.url.find('#'), std::string::npos);
    EXPECT_EQ(result.value.headers.at("content-type"), "application/json");
    const auto root = http::get(base, {.params = {{"x", "root"}}});
    ASSERT_TRUE(root.ok);
    EXPECT_EQ(root.value.text(), "root");
}

TEST_F(HttpClientTest, SendsJsonFormAndBinary)
{
    const auto json = http::post(base + "/echo", {.json = SZX::COMMON::Json{{"answer", 42}}});
    ASSERT_TRUE(json.ok) << json.error;
    const auto echoed = json.value.json();
    ASSERT_TRUE(echoed.ok);
    EXPECT_EQ(echoed.value["body"], "{\"answer\":42}");
    EXPECT_EQ(echoed.value["headers"]["Content-Type"], "application/json");
    const auto form = http::post(base + "/echo", {.form = HttpParameters{{"name", "a b&c"}, {"x", "+"}}});
    ASSERT_TRUE(form.ok);
    EXPECT_EQ(form.value.json().value["body"], "name=a%20b%26c&x=%2B");
    const std::string bytes{"a\0b", 3};
    const auto binary = http::post(base + "/binary", {.data = bytes});
    ASSERT_TRUE(binary.ok);
    EXPECT_EQ(binary.value.body, bytes);
}

TEST_F(HttpClientTest, SupportsMethodsAndHeadWithoutBody)
{
    auto created = HttpClient::Create();
    ASSERT_TRUE(created.ok);
    auto &client = *created.value;
    for (const auto method : {"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"})
    {
        auto result = client.request(method, base + "/echo");
        ASSERT_TRUE(result.ok) << method << ':' << result.error;
        EXPECT_EQ(result.value.json().value["method"], method);
    }
    EXPECT_TRUE(client.get(base + "/echo").ok);
    EXPECT_TRUE(client.post(base + "/echo").ok);
    EXPECT_TRUE(client.put(base + "/echo").ok);
    EXPECT_TRUE(client.patch(base + "/echo").ok);
    EXPECT_TRUE(client.del(base + "/echo").ok);
    EXPECT_TRUE(client.options(base + "/echo").ok);
    const auto head = client.head(base + "/large", {.maxResponseBytes = 1});
    ASSERT_TRUE(head.ok);
    EXPECT_TRUE(head.value.body.empty());
    EXPECT_EQ(head.value.headers.at("content-length"), "4096");
}

TEST_F(HttpClientTest, KeepsHttpErrorsAndDoesNotFollowRedirects)
{
    const auto result = http::get(base + "/failure");
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.value.statusCode, 422);
    EXPECT_FALSE(result.value.ok());
    EXPECT_EQ(result.value.body, "invalid");
    EXPECT_EQ(result.value.json().error, static_cast<int>(NetworkError::ProtocolError));
    const auto redirect = http::get(base + "/redirect");
    ASSERT_TRUE(redirect.ok);
    EXPECT_EQ(redirect.value.statusCode, 302);
    EXPECT_EQ(redirect.value.headers.at("location"), "/echo");
}

TEST_F(HttpClientTest, RejectsInvalidInputAndHeaderInjection)
{
    for (const auto url : {"", "/relative", "ftp://host/", "http://user:pass@host/", "http://host:0/", "http://host/\r\nX:evil"})
        EXPECT_EQ(http::get(url).error, static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_EQ(http::request("INVALID", base).error, static_cast<int>(NetworkError::InvalidArgument));
    for (const auto name : {"Host", "Content-Length", "Transfer-Encoding", "Expect", "Connection", "bad header"})
        EXPECT_EQ(http::get(base, {.headers = {{name, "evil"}}}).error, static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_EQ(http::get(base, {.headers = {{"X-Test", "a\r\nb"}}}).error, static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_EQ(http::get(base, {.data = "raw", .json = SZX::COMMON::Json::object()}).error,
              static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_EQ(http::get(base, {.timeoutSeconds = 0}).error, static_cast<int>(NetworkError::InvalidArgument));
    EXPECT_FALSE(HttpClient::Create({.workerCount = 0}).ok);
    EXPECT_FALSE(HttpClient::Create({.maxPendingRequests = 0}).ok);
    #if !SZX_NETWORK_TEST_TLS
    EXPECT_EQ(http::get("https://localhost/").error, static_cast<int>(NetworkError::TlsUnavailable));
    #endif
}

TEST_F(HttpClientTest, MapsTimeoutConnectionFailureAndSizeLimit)
{
    const auto large = http::get(base + "/large", {.maxResponseBytes = 32});
    EXPECT_EQ(large.error, static_cast<int>(NetworkError::MessageTooLarge));
    EXPECT_TRUE(large.message.empty());
    const auto slow = http::get(base + "/slow", {.timeoutSeconds = 1});
    EXPECT_EQ(slow.error, static_cast<int>(NetworkError::ReadTimeout));
    ASSERT_TRUE(server->stop().ok);
    const auto closed = http::get(base);
    EXPECT_EQ(closed.error, static_cast<int>(NetworkError::ConnectFailed));
    EXPECT_TRUE(closed.message.empty());
}

TEST_F(HttpClientTest, AsyncReturnsTypedResultsAndAllConvenienceMethodsLink)
{
    auto created = HttpClient::Create();
    ASSERT_TRUE(created.ok);
    auto &client = *created.value;
    std::vector<SZX::COMMON::Result<HttpClientFuture>> submissions;
    submissions.push_back(client.getAsync(base + "/echo"));
    submissions.push_back(client.postAsync(base + "/echo", {.json = SZX::COMMON::Json{{"async", true}}}));
    submissions.push_back(client.putAsync(base + "/echo"));
    submissions.push_back(client.patchAsync(base + "/echo"));
    submissions.push_back(client.delAsync(base + "/echo"));
    submissions.push_back(client.headAsync(base + "/echo"));
    submissions.push_back(client.optionsAsync(base + "/echo"));
    for (auto &submitted : submissions)
    {
        ASSERT_TRUE(submitted.ok);
        ASSERT_EQ(submitted.value.wait_for(5s), std::future_status::ready);
        EXPECT_TRUE(submitted.value.get().ok);
    }
    auto bad = client.getAsync("not a URL");
    ASSERT_TRUE(bad.ok);
    EXPECT_EQ(bad.value.get().error, static_cast<int>(NetworkError::InvalidArgument));
}

TEST_F(HttpClientTest, QueueBoundAndDestructionDrainAcceptedTasks)
{
    auto created = HttpClient::Create({.workerCount = 1, .maxPendingRequests = 1});
    ASSERT_TRUE(created.ok);
    auto first = created.value->getAsync(base + "/slow");
    ASSERT_TRUE(first.ok);
    EXPECT_EQ(first.value.wait_for(0ms), std::future_status::timeout);
    auto full = created.value->getAsync(base + "/echo");
    EXPECT_EQ(full.error, static_cast<int>(NetworkError::SendQueueFull));
    created.value.reset();
    ASSERT_EQ(first.value.wait_for(0ms), std::future_status::ready);
    EXPECT_TRUE(first.value.get().ok);
}

TEST_F(HttpClientTest, AsyncRunsConcurrentlyAndReturnsCapacity)
{
    auto created = HttpClient::Create({.workerCount = 2, .maxPendingRequests = 2});
    ASSERT_TRUE(created.ok);
    auto slow  = created.value->getAsync(base + "/slow");
    auto quick = created.value->getAsync(base + "/echo");
    ASSERT_TRUE(slow.ok && quick.ok);
    ASSERT_EQ(quick.value.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(quick.value.get().ok);
    EXPECT_EQ(slow.value.wait_for(0ms), std::future_status::timeout);
    auto reused = created.value->getAsync(base + "/failure");
    ASSERT_TRUE(reused.ok);
    auto result = reused.value.get();
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.value.statusCode, 422);
    EXPECT_TRUE(slow.value.get().ok);
}

TEST(HttpClientWireTest, ParsesChunkedEofCookiesAndRejectsTruncation)
{
    const auto *endpoint = std::getenv("SZX_TEST_HTTP_WIRE_URL");
    if (!endpoint)
        GTEST_SKIP() << "Run scripts/test_network_http.py for raw HTTP fixtures";
    const std::string base = endpoint;
    for (const auto path : {"/chunked", "/eof"})
    {
        const auto result = http::get(base + path);
        ASSERT_TRUE(result.ok) << path << ':' << result.error;
        EXPECT_EQ(result.value.body, "hello");
    }
    for (const auto path : {"/hints-combined", "/hints-split"})
    {
        SCOPED_TRACE(path);
        const auto result = http::get(base + path);
        EXPECT_TRUE(result.ok) << result.error;
        if (!result.ok)
            continue;
        EXPECT_EQ(result.value.statusCode, 200);
        EXPECT_EQ(result.value.body, "hello");
        EXPECT_FALSE(result.value.headers.contains("Link"));
        EXPECT_TRUE(result.value.setCookies.empty());
    }
    for (const auto path : {"/not-modified-small", "/not-modified-large", "/no-content"})
    {
        SCOPED_TRACE(path);
        const auto result = http::get(base + path, {.maxResponseBytes = 16});
        EXPECT_TRUE(result.ok) << result.error;
        if (!result.ok)
            continue;
        EXPECT_EQ(result.value.statusCode, std::string_view(path) == "/no-content" ? 204 : 304);
        EXPECT_TRUE(result.value.body.empty());
        if (result.value.statusCode == 304)
            EXPECT_TRUE(result.value.headers.contains("Content-Length"));
    }
    for (const auto path : {"/truncated", "/malformed", "/hints-truncated", "/hints-only"})
        EXPECT_EQ(http::get(base + path).error, static_cast<int>(NetworkError::ProtocolError)) << path;
    EXPECT_EQ(http::get(base + "/chunked", {.maxResponseBytes = 3}).error, static_cast<int>(NetworkError::MessageTooLarge));
    const auto cookies = http::get(base + "/cookies");
    ASSERT_TRUE(cookies.ok);
    ASSERT_EQ(cookies.value.setCookies.size(), 2u);
    EXPECT_NE(cookies.value.setCookies[0].find("first=1"), std::string::npos);
    EXPECT_NE(cookies.value.setCookies[1].find("second=2"), std::string::npos);
}

    #if SZX_NETWORK_TEST_TLS
TEST(HttpClientTlsTest, VerifiesCertificateTrustAndIpAddressForSyncAndAsync)
{
    const auto *trusted = std::getenv("SZX_TEST_HTTPS_URL");
    const auto *wrong   = std::getenv("SZX_TEST_HTTPS_WRONG_HOST_URL");
    const auto *ca      = std::getenv("SZX_TEST_HTTPS_CA");
    if (!trusted || !wrong || !ca)
        GTEST_SKIP() << "Run scripts/test_network_tls.py to supply temporary certificates";
    EXPECT_EQ(http::get(trusted).error, static_cast<int>(NetworkError::TlsHandshakeFailed));
    auto accepted = http::get(trusted, {.caFile = ca});
    ASSERT_TRUE(accepted.ok) << accepted.error;
    EXPECT_EQ(accepted.value.text(), "tls-ok");
    EXPECT_EQ(http::get(wrong, {.caFile = ca}).error, static_cast<int>(NetworkError::TlsHandshakeFailed));
    EXPECT_TRUE(http::get(wrong, {.verifyPeer = false}).ok);
    auto client = HttpClient::Create();
    ASSERT_TRUE(client.ok);
    auto future = client.value->getAsync(trusted, {.caFile = ca});
    ASSERT_TRUE(future.ok);
    EXPECT_TRUE(future.value.get().ok);
}
    #endif
#else
TEST(HttpClientDisabledTest, ReportsDisabledBackend)
{
    EXPECT_EQ(HttpClient::Create().error, static_cast<int>(NetworkError::HttpClientUnavailable));
    EXPECT_EQ(http::get("http://localhost/").error, static_cast<int>(NetworkError::HttpClientUnavailable));
}
#endif
