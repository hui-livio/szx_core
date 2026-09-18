#include <type_traits>
#include "SzxNetwork/HttpClient.h"
#include "SzxNetwork/HttpServer.h"
#include "SzxNetwork/TcpClient.h"
#include "SzxNetwork/TcpServer.h"

static_assert(std::is_same_v<SZX::NETWORK::HttpServer, SZX::NETWORK::v1::HttpServer>);
static_assert(std::is_same_v<SZX::NETWORK::HttpClient, SZX::NETWORK::v1::HttpClient>);
static_assert(std::is_same_v<SZX::NETWORK::TcpClient, SZX::NETWORK::v1::TcpClient>);
static_assert(std::is_same_v<SZX::NETWORK::TcpServer, SZX::NETWORK::v1::TcpServer>);

struct ClientEvents final : SZX::NETWORK::TcpClientCallbacks
{
    std::promise<void> connected;
    void onConnection(bool ready) override
    {
        if (ready)
            connected.set_value();
    }
    void onMessage(std::string_view) override
    {
    }
    void onError(SZX::NETWORK::NetworkError) override
    {
    }
};
int main()
{
    auto tcpServer = SZX::NETWORK::TcpServer::Create();
    if (!tcpServer.ok || !tcpServer.value->start().ok)
        return 105;
    auto tcpCallbacks = std::make_shared<ClientEvents>();
    auto connected    = tcpCallbacks->connected.get_future();
    auto tcpClient = SZX::NETWORK::TcpClient::Create({.remote = {"127.0.0.1", tcpServer.value->port()}}, std::move(tcpCallbacks));
    if (!tcpClient.ok || !tcpClient.value->start().ok || connected.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
        return 106;
    if (!tcpClient.value->stop().ok || !tcpServer.value->stop().ok)
        return 107;
    auto created = SZX::NETWORK::HttpServer::Create({});
    if (!created.ok)
        return created.error;
    auto &server      = *created.value;
    const auto before = server.useBeforeRouting([](auto &request, const auto &next) { return next(request); });
    if (!before.ok)
        return before.error;
    const auto limited = server.setMaxConnections(8);
    if (!limited.ok)
        return limited.error;
    const auto errors =
        server.onRequestError([](const auto &, int, auto) { return SZX::NETWORK::HttpServerResponse{.body = "custom error"}; });
    if (!errors.ok)
        return errors.error;
    const auto files = SZX::NETWORK::HttpServer::CreateStaticHandler(std::filesystem::current_path());
    if (!files.ok || files.value({.method = "POST", .path = "/"}).statusCode != 404)
        return 101;
    auto registered = server.get("/hello", [](const auto &) { return SZX::NETWORK::HttpServerResponse{.body = "hello"}; });
    if (!registered.ok)
        return registered.error;
    auto started = server.start();
    if (!started.ok)
        return started.error;
    if (!server.running() || server.port() == 0)
        return 100;
    auto client = SZX::NETWORK::HttpClient::Create();
    if (client.ok)
    {
        auto reply = client.value->get("http://127.0.0.1:" + std::to_string(server.port()) + "/hello");
        if (!reply.ok || reply.value.text() != "hello")
            return 102;
        auto pending = client.value->getAsync("http://127.0.0.1:" + std::to_string(server.port()) + "/hello");
        if (!pending.ok || !pending.value.get().ok)
            return 103;
    }
    else if (client.error != static_cast<int>(SZX::NETWORK::NetworkError::HttpClientUnavailable))
        return 104;
    auto stopped = server.stop();
    return stopped.ok ? 0 : stopped.error;
}
