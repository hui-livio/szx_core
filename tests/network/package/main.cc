#include <type_traits>
#include "SzxNetwork/HttpClient.h"
#include "SzxNetwork/HttpServer.h"

static_assert(std::is_same_v<SZX::NETWORK::HttpServer, SZX::NETWORK::v1::HttpServer>);
static_assert(std::is_same_v<SZX::NETWORK::HttpClient, SZX::NETWORK::v1::HttpClient>);

int main()
{
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
