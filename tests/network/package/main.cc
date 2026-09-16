#include <type_traits>
#include "SzxNetwork/HttpServer.h"

static_assert(std::is_same_v<SZX::NETWORK::HttpServer, SZX::NETWORK::v1::HttpServer>);

int main()
{
    auto created = SZX::NETWORK::HttpServer::Create({});
    if (!created.ok)
        return created.error;
    auto &server       = *created.value;
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
    auto stopped = server.stop();
    return stopped.ok ? 0 : stopped.error;
}
